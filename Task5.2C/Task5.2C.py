import tkinter as tk
from tkinter import ttk
import RPi.GPIO as GPIO

# GPIO setup (BCM numbering)
LED_PINS = {
    "living room": 18,   # physical pin 12
    "bathroom": 13,      # physical pin 33
    "closet": 12         # physical pin 32
}

PWM_FREQUENCY = 100

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)

for pin in LED_PINS.values():
    GPIO.setup(pin, GPIO.OUT)

pwm_leds = {}
for name, pin in LED_PINS.items():
    pwm_leds[name] = GPIO.PWM(pin, PWM_FREQUENCY)
    pwm_leds[name].start(0)

brightness_vars = {}
timer_vars = {}
timer_jobs = {}

def clamp(value):
    return max(0, min(100, int(float(value))))

def all_off():
    for pwm in pwm_leds.values():
        pwm.ChangeDutyCycle(0)
    status_label.config(text="All LEDs are OFF")

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
    brightness = clamp(brightness_vars[selected].get())

    for name, pwm in pwm_leds.items():
        pwm.ChangeDutyCycle(brightness if name == selected else 0)

    status_label.config(text=f"{selected.title()} LED is ON at {brightness}%")
    selected_room.set(selected)

def on_radio_change():
    set_room(selected_room.get())

def submit_text():
    set_room(room_entry.get())

def on_slider_change(room_name, value):
    brightness_vars[room_name].set(clamp(value))
    if selected_room.get() == room_name:
        set_room(room_name)

def timer_set_room(room_name):
    timer_jobs[room_name] = None
    set_room(room_name)

def start_timer(room_name):
    try:
        seconds = float(timer_vars[room_name].get())
        if seconds < 0:
            raise ValueError
    except ValueError:
        status_label.config(text=f"Invalid timer for {room_name.title()}. Enter seconds.")
        return

    if timer_jobs.get(room_name) is not None:
        try:
            root.after_cancel(timer_jobs[room_name])
        except Exception:
            pass

    delay_ms = int(seconds * 1000)
    timer_jobs[room_name] = root.after(delay_ms, lambda r=room_name: timer_set_room(r))
    status_label.config(text=f"{room_name.title()} timer started for {seconds:.1f} second(s)")

def switch_mode():
    mode = input_mode.get()

    radio_frame.pack_forget()
    text_frame.pack_forget()

    if mode == "radio":
        radio_frame.pack(pady=10)
    else:
        text_frame.pack(pady=10)

def close_app():
    for job in timer_jobs.values():
        if job is not None:
            try:
                root.after_cancel(job)
            except Exception:
                pass

    all_off()

    for pwm in pwm_leds.values():
        pwm.stop()

    GPIO.cleanup()
    root.destroy()

# GUI setup
root = tk.Tk()
root.title("Room Light Controller")
root.geometry("560x560")
root.resizable(False, False)

title_label = ttk.Label(root, text="Room Light Controller", font=("Arial", 16, "bold"))
title_label.pack(pady=10)

input_mode = tk.StringVar(value="radio")
selected_room = tk.StringVar(value="living room")

for room in LED_PINS:
    brightness_vars[room] = tk.IntVar(value=100)
    timer_vars[room] = tk.StringVar(value="0")

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

pwm_frame = ttk.LabelFrame(root, text="Brightness + Timer Control")
pwm_frame.pack(padx=15, pady=10, fill="x")

for room in LED_PINS:
    row = ttk.Frame(pwm_frame)
    row.pack(fill="x", padx=8, pady=8)

    ttk.Label(row, text=room.title(), width=12).grid(row=0, column=0, padx=5, pady=5, sticky="w")

    slider = tk.Scale(
        row,
        from_=0,
        to=100,
        orient="horizontal",
        length=180,
        variable=brightness_vars[room],
        command=lambda value, r=room: on_slider_change(r, value)
    )
    slider.grid(row=0, column=1, padx=5, pady=5)

    ttk.Label(row, text="Timer (s):").grid(row=0, column=2, padx=5, pady=5)
    ttk.Entry(row, textvariable=timer_vars[room], width=7).grid(row=0, column=3, padx=5, pady=5)

    ttk.Button(
        row,
        text="Start Timer",
        command=lambda r=room: start_timer(r)
    ).grid(row=0, column=4, padx=5, pady=5)

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

