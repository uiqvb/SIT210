# Task 1.1P: Switching ON Lights (Modular Programming)

## System Overview
This project is a smart lighting system designed for an assisted living home. The system is built to help a resident named Linda safely enter her home at night. 

When the user activates a physical slide switch, the system instantly turns on both a Porch Light and a Hallway Light. To automate the process and save energy, the Porch Light automatically turns off after 30 seconds, while the Hallway Light remains on for a total of 60 seconds to allow the user time to get inside. 

The system was developed using an Arduino Nano 33 IoT, LEDs, and a physical switch. The final code uses a non-blocking `millis()` timer and software debouncing to ensure the hardware responds instantly to user input without freezing or glitching.

## Circuit Schematic
*(Below is the visualization of the wiring of the hardware made in tinkercad)*

<img width="527" height="363" alt="image" src="https://github.com/user-attachments/assets/a443cfed-7c42-408c-b655-d42691ff6fc7" />

## Code Overview (Modular Functions)
The code utilizes a modular programming approach, breaking the system into manageable chunks. Here is a general overview of what each part of the code is doing:

* **`setup()`**: Configures the hardware at startup, setting the LED pins as outputs and the switch pin as an input with an internal pull-up resistor.
* **`loop()`**: The main brain of the program. It constantly checks the debounced state of the switch, starts the lighting sequence if the switch is ON, and continuously checks the `millis()` stopwatch to turn off the lights at the correct 30-second and 60-second intervals.
* **Debouncing Logic (Inside Loop)**: Reads the physical switch and waits 50 milliseconds to confirm the reading, filtering out false triggers caused by microscopic hardware vibrations.
* **`startLights()`**: A dedicated helper function that triggers both the porch and hallway LEDs to turn ON.
* **`turnOffPorchLight()` / `turnOffHallLight()`**: Helper functions that turn off their respective LEDs.
* **`turnOffEverything()`**: An emergency override function that instantly shuts off all LEDs if the switch is deactivated early.


