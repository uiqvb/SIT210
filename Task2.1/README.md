# Task 2.1P: Sending Temperature and Light Data to the Web

## System Overview
This project is a room condition monitoring system designed for Linda’s assisted living environment. The system uses an Arduino Nano 33 IoT, a DHT11 sensor, and an analogue light sensor to monitor important environmental conditions inside the room.

The Arduino reads temperature data from the DHT11 sensor and light intensity data from the analogue light sensor, then uploads the readings to ThingSpeak every 30 seconds using Wi-Fi. In addition to the core task requirements, the system also includes humidity monitoring and an alarm value/status field to make the solution more useful in a smart home setting.

The project demonstrates how embedded systems can collect both digital and analogue sensor data and send that information to a web platform for remote viewing and monitoring.

## Circuit Schematic
*(Below is the visualization of the wiring of the hardware made in Tinkercad)*

<img width="1134" height="732" alt="image" src="https://github.com/user-attachments/assets/da5ca4e8-59de-4bca-a94b-34576e8fc212" />


## ThingSpeak Dashboard
*(Below is the web dashboard used to display the uploaded sensor data)*
<img width="948" height="606" alt="image" src="https://github.com/user-attachments/assets/58353ac4-47b4-46fe-9417-5f6b3a971de2" />

## Code Overview (Modular Functions)
The code utilizes a modular programming approach, breaking the system into manageable chunks. Here is a general overview of what each part of the code is doing:

* **`setup()`**: Initializes the Serial Monitor, starts the DHT11 sensor, connects the board to Wi-Fi, and prepares ThingSpeak communication.
* **`loop()`**: Continuously reads the sensor values, checks alarm conditions, prints the readings to the Serial Monitor, and uploads the data to ThingSpeak every 30 seconds.
* **`connectToWiFi()`**: Connects the Arduino Nano 33 IoT to the local Wi-Fi network so it can send data to the web.
* **`readTemperatureC()`**: Reads the room temperature from the DHT11 sensor.
* **`readHumidityPercent()`**: Reads the humidity value from the DHT11 sensor.
* **`readLightRaw()`**: Reads the analogue light sensor value. The code averages multiple samples to reduce noise and improve reading stability.
* **`calculateAlarmValue()`**: Checks whether the current readings exceed the defined thresholds and determines whether an alarm condition should be triggered.
* **`buildAlarmMessage()`**: Creates a text-based status or alarm message that can be sent along with the uploaded data.
* **`printReadings()`**: Displays the temperature, humidity, light, and alarm values in the Serial Monitor for local testing and debugging.
* **`uploadToThingSpeak()`**: Uploads the sensor readings to the correct ThingSpeak fields so they can be stored and visualised online.

## Signal Types Used in the System
This project demonstrates the use of both analogue and digital signals:

* **Analogue input**: The light sensor produces a changing voltage depending on ambient light, which the Arduino reads using `analogRead()`.
* **Digital input**: The DHT11 sensor sends temperature and humidity readings digitally through its data pin.

This combination shows how embedded systems handle different forms of real-world sensor input.

## Testing and Results
The system was tested by changing the room’s lighting conditions and observing how the uploaded data changed on ThingSpeak. For example, when extra light from a phone torch was directed at the sensor, the light reading increased immediately. The temperature readings changed more gradually over time.

The ThingSpeak dashboard confirmed that the Arduino successfully uploaded sensor data every 30 seconds, demonstrating correct operation of the sensors, Wi-Fi connection, and web communication.

## Files
- `script.ino` – Main Arduino code for the project

## Author
Manit Khera
