# Task 3.2C: Gesture-Controlled Smart Home (Deakin Detour)

## System Overview
This project is an interactive gesture-controlled system designed to enhance Linda’s smart home environment. Using an Arduino Nano 33 IoT and an HC-SR04 ultrasonic sensor, the system detects specific hand gestures—Waves and Pats—to trigger actions both locally and across the network via MQTT.

The system distinguishes between a "Wave" (holding a hand near the sensor) and a "Pat" (quick double-tap motion). These gestures are published to an MQTT broker, allowing for remote monitoring and integration with services like Node-RED. In this implementation, a Wave gesture turns on local LEDs, while a Pat gesture turns them off. Additionally, a Node-RED flow tracks the number of Wave gestures and sends an email notification to a carer if three waves are detected, ensuring Linda's safety through non-contact interaction.

## Circuit Schematic
*(Below is the visualization of the wiring of the hardware)*

[Insert Circuit Schematic Image Here]

## Node-RED Flow
*(Below is the Node-RED flow used to process MQTT messages and trigger email alerts)*

[Insert Node-RED Flow Image Here]

## Code Overview (Modular Functions)
The code utilizes a modular programming approach, breaking the system into manageable chunks. Here is a general overview of what each part of the code is doing:

* **`setup()`**: Initializes the Serial Monitor, configures the ultrasonic sensor pins and LED pins, and prepares the Wi-Fi and MQTT client.
* **`loop()`**: The main brain of the program. It maintains network connections, processes incoming MQTT messages, and continuously polls the ultrasonic sensor for gesture detection.
* **`maintainWiFi()` / `maintainMQTT()`**: Robust reconnection logic that ensures the system stays online even if the network connection is lost.
* **`readDistanceCm()`**: A helper function that triggers the ultrasonic pulse and calculates the distance to an object in centimeters.
* **`detectGesture()`**: The core logic that analyzes distance readings over time. It identifies a **Wave** if an object is held near the sensor for a set duration, and a **Pat** if it detects a quick double-tap motion.
* **`publishWave()` / `publishPat()`**: Sends the detected gesture as an MQTT message to the broker and provides instant local feedback by toggling the LEDs.
* **`mqttCallback()`**: Handles incoming messages from the broker, allowing the device to respond to gestures (e.g., turning LEDs ON when a Wave is received on the topic).
* **`ledsOn()` / `ledsOff()`**: Simple helper functions to control the physical status indicators.

## Communication Protocols Used
This project demonstrates several important embedded systems communication methods:

* **Ultrasonic Pulse Timing**: Used to calculate distance for gesture interpretation.
* **Wi-Fi**: Used to connect the Arduino Nano 33 IoT to the internet.
* **MQTT**: A lightweight messaging protocol used to publish gesture events (`ES/Wave`, `ES/Pat`) to the EMQX broker.
* **Node-RED Logic**: Used to monitor the MQTT stream, maintain a "wave count" in the flow context, and trigger email notifications via Gmail/SMTP.

## Testing and Results
The system was tested by performing various hand movements in front of the ultrasonic sensor:
* **Wave Detection**: A steady hand held within 12cm for more than 350ms correctly triggered a `Wave` event, turning on the LEDs and publishing to the broker.
* **Pat Detection**: Two quick taps within 800ms correctly triggered a `Pat` event, turning off the LEDs.
* **Integration**: The Node-RED flow successfully tracked the Wave count, resetting after the third wave and dispatching an email notification to the designated carer.

Testing confirmed that the ultrasonic sensor, MQTT communication, and Node-RED backend worked together to create a responsive and reliable monitoring solution.

## Files
- `script.ino` – Main Arduino code for gesture detection and MQTT communication.
- `node-red.ino` – Logic for the Node-RED function node to handle wave counting and email alerts.

## Author
Manit Khera
