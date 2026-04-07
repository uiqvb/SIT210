import tkinter as tk
from gpiozero import PWMLED

living_room = PWMLED(18)
bathroom = PWMLED(13)
closet = PWMLED(19)

timer_job = None

def all_off():
    living_room.off()
    bathroom.off()
    closet.off()

def apply_timed_change():
    global timer_job
    living_room.value = living_scale.get() / 100
    bathroom.value = bathroom_scale.get() / 100
    closet.value = closet_scale.get() / 100

    status_label.config(
        text=(
            f"Applied: Living {living_scale.get()}% | "
            f"Bathroom {bathroom_scale.get()}% | "
            f"Closet {closet_scale.get()}%"
        )
    )
    timer_job = None

def start_timer():
    global timer_job

    if timer_job is not None:
        window.after_cancel(timer_job)
        timer_job = None

    try:
        delay_seconds = int(delay_entry.get())
        if delay_seconds < 0:
            raise ValueError
    except ValueError:
        status_label.config(text="Enter a whole number of seconds")
        return

    all_off()
    status_label.config(text=f"Timed change starts in {delay_seconds} second(s)")
    timer_job = window.after(delay_seconds * 1000, apply_timed_change)

def all_on_now():
    if timer_job is not None:
        window.after_cancel(timer_job)
    living_room.value = living_scale.get() / 100
    bathroom.value = bathroom_scale.get() / 100
    closet.value = closet_scale.get() / 100
    status_label.config(text="Applied immediately")

def clear_all():
    global timer_job
    if timer_job is not None:
        window.after_cancel(timer_job)
        timer_job = None
    all_off()
    status_label.config(text="All lights are OFF")

def exit_app():
    global timer_job
    if timer_job is not None:
        window.after_cancel(timer_job)
    all_off()
    window.destroy()

window = tk.Tk()
window.title("3 LED PWM Timer Control")
window.geometry("430x400")

title_label = tk.Label(window, text="Timed Light Intensity Control", font=("Arial", 14))
title_label.pack(pady=10)

living_scale = tk.Scale(
    window,
    from_=0,
    to=100,
    orient="horizontal",
    length=300,
    label="Living Room Intensity"
)
living_scale.set(100)
living_scale.pack(pady=5)

bathroom_scale = tk.Scale(
    window,
    from_=0,
    to=100,
    orient="horizontal",
    length=300,
    label="Bathroom Intensity"
)
bathroom_scale.set(100)
bathroom_scale.pack(pady=5)

closet_scale = tk.Scale(
    window,
    from_=0,
    to=100,
    orient="horizontal",
    length=300,
    label="Closet Intensity"
)
closet_scale.set(100)
closet_scale.pack(pady=5)

delay_frame = tk.Frame(window)
delay_frame.pack(pady=10)

delay_label = tk.Label(delay_frame, text="Start after (seconds):")
delay_label.pack(side="left", padx=5)

delay_entry = tk.Entry(delay_frame, width=8)
delay_entry.pack(side="left", padx=5)
delay_entry.insert(0, "3")

start_button = tk.Button(window, text="Start Timed Change", command=start_timer, width=18)
start_button.pack(pady=5)

apply_now_button = tk.Button(window, text="Apply Now", command=all_on_now, width=18)
apply_now_button.pack(pady=5)

all_off_button = tk.Button(window, text="All Off", command=clear_all, width=18)
all_off_button.pack(pady=5)

exit_button = tk.Button(window, text="Exit", command=exit_app, width=18)
exit_button.pack(pady=5)

status_label = tk.Label(window, text="All lights are OFF")
status_label.pack(pady=10)

window.protocol("WM_DELETE_WINDOW", exit_app)

all_off()
window.mainloop()