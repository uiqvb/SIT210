# Task 3.1P: Create Trigger/Notification Based on Sensor Data

## System Overview
This project is a light-triggered notification system designed to monitor sunlight exposure in Linda’s terrarium. The system uses an Arduino Nano 33 IoT, a BH1750 light sensor, HiveMQ Cloud, Node-RED, and Gmail to detect sunlight events and generate notifications.

The BH1750 sensor continuously measures light intensity in lux and sends the readings to the Arduino through the I2C interface. The Arduino then decides whether the terrarium is currently receiving sunlight. When the light condition changes, the Arduino publishes an MQTT message to HiveMQ Cloud. Node-RED subscribes to that MQTT topic and sends an email notification through Gmail.

The system sends one notification when sunlight starts and another when sunlight stops. This creates a complete end-to-end sensing, communication, and notification workflow.

## Circuit Schematic
*(Below is the visualization of the wiring of the hardware made in Wokwi / schematic tool)*

<img width="654" height="463" alt="Screenshot 2026-03-21 091210" src="https://github.com/user-attachments/assets/98500cfe-6e84-4959-8f41-3c70ce0c39c1" />




## System Architecture
*(Below is the overall communication flow of the system)*

<img width="1221" height="556" alt="Screenshot 2026-03-17 111640" src="https://github.com/user-attachments/assets/9aec5e7f-84d4-4a76-9ec9-e6ecd34bd0b9" />


## Node-RED Flow
*(Below is the Node-RED flow used to process MQTT messages and trigger email alerts)*

<img width="975" height="376" alt="Screenshot 2026-03-21 083710" src="https://github.com/user-attachments/assets/2673f678-277f-4624-976e-47504c4a1ba4" />



## HiveMQ Cloud Evidence
*(Below is proof that MQTT messages reached the broker successfully)*
<img width="1919" height="1079" alt="Screenshot 2026-03-21 073647" src="https://github.com/user-attachments/assets/3ccd4bc4-edbf-47bb-9aa5-cab3913c0714" />

## Serial Monitor Evidence
*(Below is the Serial Monitor output used during testing)*
<img width="1919" height="1079" alt="Screenshot 2026-03-21 073629" src="https://github.com/user-attachments/assets/14995eb8-218b-444b-9f53-6cbb59aae163" />

## Code Overview
The code utilizes a modular and event-driven programming approach. Here is a general overview of what each part of the code is doing:

* **`setup()`**: Initializes the Serial Monitor, starts the BH1750 light sensor, connects the Arduino to Wi-Fi, and connects to the MQTT broker.
* **`loop()`**: Continuously reads the current lux value, checks whether the sunlight state has changed, maintains the MQTT connection, and publishes events only when needed.
* **BH1750 Sensor Reading Logic**: Reads ambient light levels in lux using the I2C interface.
* **Sunlight Threshold Logic**: Compares the lux reading against defined thresholds to determine whether the terrarium is in a `NO SUN` or `SUNLIGHT` state.
* **State Change Detection**: Tracks the previous and current light state so that the system only reacts when sunlight starts or stops.
* **MQTT Publishing Logic**: Publishes `SUNLIGHT_STARTED` when sunlight begins and `SUNLIGHT_STOPPED` when sunlight ends.
* **Wi-Fi / MQTT Reconnection Logic**: Keeps the device connected to the network and broker so it can continue sending events reliably.
* **Serial Debug Output**: Prints lux readings, state information, and publication results to the Serial Monitor for testing and troubleshooting.

## Communication Protocols Used
This project demonstrates several important embedded systems communication methods:

* **I2C**: Used between the BH1750 light sensor and the Arduino Nano 33 IoT
* **Wi-Fi**: Used to connect the Arduino to the internet
* **MQTT**: Used to publish event messages to HiveMQ Cloud
* **Node-RED Email Flow**: Used to convert MQTT events into email notifications

## Testing and Results
The system was tested by changing the amount of light reaching the BH1750 sensor. When the sensor detected strong light, the Arduino classified the condition as sunlight and published `SUNLIGHT_STARTED`. When the light was removed or reduced below the threshold, the Arduino published `SUNLIGHT_STOPPED`.

Testing confirmed that:
* the sensor readings were correctly interpreted
* MQTT messages were successfully published to HiveMQ Cloud
* Node-RED received the messages correctly
* Gmail notifications were sent for both sunlight events

This demonstrated correct end-to-end system behaviour across sensing, communication, and notification.

## Files
- Main Arduino sketch for BH1750 sensing and MQTT publishing
- Node-RED flow for email notifications

## Author
Manit Khera
