import tkinter as tk
from tkinter import ttk, messagebox
import RPi.GPIO as GPIO

# GPIO setup
LED_PINS = {
    "living room": 17,   # physical pin 11
    "bathroom": 27,      # physical pin 13
    "closet": 22         # physical pin 15
}

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)

for pin in LED_PINS.values():
    GPIO.setup(pin, GPIO.OUT)
    GPIO.output(pin, GPIO.LOW)

def all_off():
    for pin in LED_PINS.values():
        GPIO.output(pin, GPIO.LOW)

def set_room(room_name):
    room = room_name.strip().lower()

    aliases = {
        "living": "living room",
        "living room": "living room",
        "bath": "bathroom",
        "bathroom": "bathroom",
        "closet": "closet"
    }

    if room not in aliases:
        status_label.config(text="Invalid room. Use: living room, bathroom, or closet.")
        return

    selected = aliases[room]

    for name, pin in LED_PINS.items():
        GPIO.output(pin, GPIO.HIGH if name == selected else GPIO.LOW)

    status_label.config(text=f"{selected.title()} LED is ON")
    selected_room.set(selected)

def on_radio_change():
    set_room(selected_room.get())

def submit_text():
    set_room(room_entry.get())

def switch_mode():
    mode = input_mode.get()

    radio_frame.pack_forget()
    text_frame.pack_forget()

    if mode == "radio":
        radio_frame.pack(pady=10)
    else:
        text_frame.pack(pady=10)

def close_app():
    all_off()
    GPIO.cleanup()
    root.destroy()

# GUI setup
root = tk.Tk()
root.title("Room Light Controller")
root.geometry("420x320")
root.resizable(False, False)

title_label = ttk.Label(root, text="Room Light Controller", font=("Arial", 16, "bold"))
title_label.pack(pady=10)

input_mode = tk.StringVar(value="radio")
selected_room = tk.StringVar(value="living room")

mode_frame = ttk.LabelFrame(root, text="Choose Input Method")
mode_frame.pack(padx=15, pady=10, fill="x")

ttk.Radiobutton(
    mode_frame, text="Radio Buttons", variable=input_mode,
    value="radio", command=switch_mode
).pack(anchor="w", padx=10, pady=5)

ttk.Radiobutton(
    mode_frame, text="Text Input", variable=input_mode,
    value="text", command=switch_mode
).pack(anchor="w", padx=10, pady=5)

radio_frame = ttk.LabelFrame(root, text="Radio Button Control")
ttk.Radiobutton(
    radio_frame, text="Living Room", variable=selected_room,
    value="living room", command=on_radio_change
).pack(anchor="w", padx=10, pady=5)

ttk.Radiobutton(
    radio_frame, text="Bathroom", variable=selected_room,
    value="bathroom", command=on_radio_change
).pack(anchor="w", padx=10, pady=5)

ttk.Radiobutton(
    radio_frame, text="Closet", variable=selected_room,
    value="closet", command=on_radio_change
).pack(anchor="w", padx=10, pady=5)

text_frame = ttk.LabelFrame(root, text="Text Input Control")
room_entry = ttk.Entry(text_frame, width=28)
room_entry.pack(padx=10, pady=10)
room_entry.insert(0, "living room")

ttk.Button(text_frame, text="Turn On Selected Room", command=submit_text).pack(pady=5)

status_label = ttk.Label(root, text="Select a room to turn on its LED.")
status_label.pack(pady=10)

button_frame = ttk.Frame(root)
button_frame.pack(pady=10)

ttk.Button(button_frame, text="All Off", command=all_off).pack(side="left", padx=8)
ttk.Button(button_frame, text="Exit", command=close_app).pack(side="left", padx=8)

switch_mode()
set_room("living room")

root.protocol("WM_DELETE_WINDOW", close_app)
root.mainloop()