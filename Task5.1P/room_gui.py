import tkinter as tk
from gpiozero import LED

living_room = LED(17)
bathroom = LED(27)
closet = LED(22)

def all_off():
    living_room.off()
    bathroom.off()
    closet.off()

def select_room():
    choice = room_var.get()
    all_off()

    if choice == 1:
        living_room.on()
    elif choice == 2:
        bathroom.on()
    elif choice == 3:
        closet.on()

def all_off_button():
    room_var.set(0)
    all_off()

def exit_app():
    all_off()
    window.destroy()

window = tk.Tk()
window.title("Room Lights")
window.geometry("300x260")

room_var = tk.IntVar(value=0)

title_label = tk.Label(window, text="Choose one room light", font=("Arial", 14))
title_label.pack(pady=10)

rb1 = tk.Radiobutton(
    window,
    text="Living Room",
    variable=room_var,
    value=1,
    command=select_room
)
rb1.pack(anchor="w", padx=30, pady=5)

rb2 = tk.Radiobutton(
    window,
    text="Bathroom",
    variable=room_var,
    value=2,
    command=select_room
)
rb2.pack(anchor="w", padx=30, pady=5)

rb3 = tk.Radiobutton(
    window,
    text="Closet",
    variable=room_var,
    value=3,
    command=select_room
)
rb3.pack(anchor="w", padx=30, pady=5)

off_button = tk.Button(window, text="All Off", command=all_off_button, width=10)
off_button.pack(pady=8)

exit_button = tk.Button(window, text="Exit", command=exit_app, width=10)
exit_button.pack(pady=8)

window.protocol("WM_DELETE_WINDOW", exit_app)

all_off()
window.mainloop()