# Evaluation Criteria Understanding Notes  
## Smart Clothesline Robot — Viva / Demo Preparation

These notes explain how the Smart Clothesline Robot maps to the project rubric and which advanced criteria can be defended during the demonstration/viva.

---

## 1. What the rubric is looking for

The rubric is not only checking whether the robot works. It is checking whether the project shows embedded-system thinking.

The main things being evaluated are:

```text
1. Does the project solve a real problem?
2. Is it a working embedded prototype?
3. Are there multiple systems working together?
4. Is there communication between devices?
5. Does the system react to sensors, environment, or user interaction?
6. Has it been tested and improved?
7. Does it show fault tolerance / robustness?
8. Does it show responsiveness / real-time behaviour?
9. Is it a complete prototype rather than a simple sensor demo?
10. Can I explain tradeoffs, limitations, and improvements?
```

---

## 2. My project in one sentence

> This project is a Smart Clothesline Robot, a proof of concept for an automated clothesline that detects rain, compares sunlight, follows a line, moves between a drying zone and a safe zone, and communicates with a Raspberry Pi dashboard using MQTT.

---

## 3. What level my project can defend

The project can defend at least:

```text
Credit:
  - Multiple systems are incorporated.
  - There is communication between systems.
  - A working prototype solves a basic real-world problem.

Distinction:
  - The system solves a real-world problem in a specific context.
  - The prototype acts based on environmental readings and user interaction.
  - The prototype has been tested and optimised for fault tolerance.

High Distinction-style criteria:
  - Robustness / fault tolerance.
  - Responsiveness / soft real-time behaviour.
  - More complete prototype with multiple sensors, motors, backend, dashboard, MQTT, Raspberry Pi, systemd, and Tailscale.
```

Important:

```text
Do not claim multi-threading as the main advanced criterion.
Claim robustness and responsiveness instead.
```

---

## 4. Clear purpose / real-world problem

The real-world problem is:

```text
Clothes left outside can get wet when the weather changes suddenly.
```

My context:

```text
Melbourne weather changes quickly.
The project explores how an embedded system could automatically react to rain and move laundry to a safe area.
```

Viva answer:

> The project solves the problem of clothes getting wet when unexpected rain occurs. Instead of building a full-size automated clothesline, I built a small robot as a proof of concept to demonstrate the sensing, decision-making, movement, and communication logic.

---

## 5. Why it is an embedded system

This is a true embedded system because the Nano 33 IoT is physically integrated with sensors and actuators and runs local control logic.

The Nano handles:

```text
rain detection
line following
zone detection
sunlight comparison
motor control
state machine
MQTT communication
```

The system is not just a web app. The web dashboard is only the monitoring/control interface.

Viva answer:

> The Nano is the embedded controller. It reads sensors and controls the motors locally. The Raspberry Pi dashboard is useful for monitoring and manual commands, but the robot behaviour is controlled by the embedded firmware.

---

## 6. Multiple systems incorporated

The project has multiple systems working together:

```text
1. Nano 33 IoT robot
2. Raspberry Pi backend/dashboard
3. Laptop browser interface
4. MQTT broker communication layer
5. Tailscale remote access layer
```

The most important two systems are:

```text
Nano 33 IoT robot
Raspberry Pi backend/dashboard
```

Viva answer:

> The project incorporates at least two systems: the Nano-based embedded robot and the Raspberry Pi backend. They communicate using MQTT, with the Pi acting as the dashboard and command publisher while the Nano controls the robot locally.

---

## 7. Communication between devices

Communication layers:

```text
Nano ↔ MQTT broker ↔ Raspberry Pi backend
Laptop browser ↔ Tailscale ↔ Raspberry Pi dashboard
Nano ↔ BH1750 sensors over I2C
Nano → L298N motor driver through GPIO
```

Main communication protocol to defend:

```text
MQTT
```

Other relevant communication:

```text
I2C for BH1750 light sensors
HTTP between browser and Flask backend
Tailscale for remote dashboard access
```

Viva answer:

> MQTT is used between the Raspberry Pi backend and the Nano. The backend publishes commands to the command topic, and the Nano publishes telemetry to the telemetry topic. The laptop does not talk to the Nano directly; it accesses the Raspberry Pi dashboard through Tailscale.

---

## 8. The system acts based on environment and user interaction

Environmental inputs:

```text
Rain sensor:
  detects wet/dry conditions

BH1750 light sensors:
  compare left and right lux values

Line sensors:
  detect track and zone markers
```

User inputs:

```text
Dashboard commands:
  MOVE_TO_SAFE
  RETURN_TO_DRYING
  STOP
  SET_ACTIVE_TRUE
  SET_ACTIVE_FALSE
```

Resulting behaviours:

```text
Rain confirmed while in DRYING:
  robot moves to SAFE

Left side brighter:
  robot rotates left

Right side brighter:
  robot rotates right

Line pattern centered:
  robot moves forward

Zone marker detected:
  robot confirms SAFE or DRYING
```

Viva answer:

> The robot acts depending on both environmental readings and user interaction. Rain can automatically trigger movement to SAFE, light readings affect sun tracking, line sensors control movement and zone detection, and dashboard buttons allow manual commands.

---

## 9. Robustness / fault tolerance

This is one of the strongest advanced criteria.

### Faults currently handled

| Fault/problem | How it is handled | Where it is handled |
|---|---|---|
| Dashboard/laptop failure | Nano still runs local safety logic | Architecture / Nano firmware |
| Tailscale failure | Only remote dashboard access is lost | Architecture |
| Wi-Fi/MQTT failure | Local Nano logic continues | Nano firmware |
| Rain sensor noise | Threshold + confirmation time | `updateRainSensor()` |
| Obvious rain telemetry fault | Backend weather fallback can recommend/send `MOVE_TO_SAFE` | Backend fallback logic |
| Line sensor flicker | Forward commit + side confirmation | `updateMoving()` |
| Temporary line loss | Line hunting and last known direction | `LINE_HUNT` / `updateLineHunt()` |
| False zone detection | Stable repeated marker counts | Marker verification logic |
| Tiny light fluctuations | BH1750 deadband | `updateSunTracking()` |
| Backend restart/reboot | `systemd` service | Raspberry Pi deployment |
| Conflicting movement commands | Ignore movement commands during critical states | MQTT command handling |

### Main robustness answer

> The system is robust because the safety-critical logic is local on the Nano. If the dashboard, laptop, Tailscale, or MQTT path fails, the robot can still read sensors and run its local state machine. Sensor noise is handled through confirmation logic, and movement reliability is improved with line hunting, forward commit, side confirmation, and stable zone detection.

---

## 10. Rain sensor fault tolerance

There are two layers:

```text
Primary:
  Nano local rain sensor logic

Secondary:
  Backend weather fallback if rain telemetry looks faulty
```

### Normal case

```text
Rain sensor detects wet
  ↓
Nano confirms rain over time
  ↓
If laundry active and state == DRYING
  ↓
Nano moves to SAFE
```

### Fault case

```text
Telemetry says rain data is missing / invalid / unknown
  ↓
Backend checks local weather
  ↓
If weather says rain likely
  ↓
Backend recommends or publishes MOVE_TO_SAFE
```

Important limitation:

```text
If the rain sensor is stuck at a believable dry value, the backend may not know it has failed.
If MQTT/backend/weather are unavailable, the backend fallback cannot send a command.
```

Viva answer:

> Rain safety has two layers. The primary layer is local on the Nano using the rain sensor and confirmation time. The secondary layer is backend weather fallback, where faulty rain telemetry can trigger a weather-based `MOVE_TO_SAFE` recommendation. This improves fault tolerance, but it is not full redundancy because it depends on telemetry, MQTT, backend, and weather availability.

---

## 11. Responsiveness

Yes, the project can defend responsiveness.

Responsiveness means the system reacts in a reasonable time to important events.

Examples:

```text
Rain sensor is checked continuously in the Nano loop.
Line following is handled locally in the Nano loop.
MQTT commands are received through mqttClient.loop().
Dashboard refreshes periodically through /api/status.
Telemetry is published periodically to the backend.
```

Important distinction:

```text
Rain response and line following:
  time-sensitive, handled locally

Dashboard updates:
  less time-critical, handled through polling
```

Viva answer:

> The system is responsive because the Nano continuously checks rain, line sensors, robot state, and MQTT communication inside its loop. The most time-sensitive behaviours, rain response and line following, are handled locally on the embedded controller rather than waiting for the dashboard.

---

## 12. Real-time behaviour

The project is best described as:

```text
soft real-time
```

Do not call it hard real-time.

### Why it is soft real-time

```text
Rain response should happen quickly.
Line following should respond quickly.
Motor decisions should be updated frequently.
Small timing delays are acceptable.
Missing a deadline is not catastrophic like in an airbag or medical device.
```

Viva answer:

> This is a soft real-time embedded system. Rain response and line following are time-sensitive, but the project does not require hard real-time guarantees. A small delay is acceptable, but the robot still needs to respond quickly enough for a practical demo.

---

## 13. Multi-threading

Do not claim this as a major feature.

### Nano firmware

The Nano code is not multi-threaded.

It uses:

```text
single continuous loop
state machine
non-blocking timing using millis()
periodic checks
```

### Backend

The Flask/MQTT backend may involve concurrent behaviour depending on deployment and MQTT client handling, but this should not be your main argument unless asked specifically.

Best answer:

> I would not claim multi-threading as my main evaluation point. The Nano firmware uses a single loop and state machine, which is common in embedded systems. My stronger advanced criteria are robustness and responsiveness.

---

## 14. More complete prototype

This project is more complete than a simple sensor demo.

It includes:

```text
physical robot chassis
two DC motors
L298N motor driver
three line sensors
rain sensor
two BH1750 light sensors
Nano 33 IoT firmware
state machine
line following
zone detection
sun tracking
rain safety
MQTT communication
Raspberry Pi Flask backend
dashboard
logging
systemd service
Tailscale remote access
weather fallback
```

Viva answer:

> The prototype is more complete because it integrates sensing, actuation, communication, backend monitoring, logging, and remote access. It is not just a sensor reading project; the robot physically moves and changes behaviour based on sensor readings and user commands.

---

## 15. Testing and optimisation

Testing was done incrementally.

Development order:

```text
1. Build chassis.
2. Test motors and L298N.
3. Add line sensors and test readings.
4. Add rain sensor and test dry/wet values.
5. Add BH1750 sensors and test lux values.
6. Build sun rotation logic.
7. Build serial movement commands.
8. Add line following.
9. Add zone detection.
10. Add MQTT communication.
11. Add Flask dashboard.
12. Deploy backend on Raspberry Pi.
13. Add systemd service.
14. Add Tailscale access.
```

Optimisations:

```text
Rain threshold + confirmation time
Line hunting
Forward commit
Side confirmation
Stable zone confirmation counts
Pulsed motor movement
Light deadband
Backend weather fallback
Systemd deployment
```

Viva answer:

> I built and tested the project incrementally. Each part was tested separately before integration. After initial movement worked, I improved reliability using confirmation logic, line recovery, and smoother motor control.

---

## 16. Faults not fully handled

These are limitations to admit honestly.

| Limitation | Why it matters | Improvement |
|---|---|---|
| Motor failure | Nano cannot verify physical movement | Add encoders/current sensing |
| Low battery | Motors and sensors may become unreliable | Add voltage monitoring |
| Rain sensor stuck at valid dry value | Backend may not detect fault | Add diagnostics/redundant sensor |
| Permanent line sensor failure | Tracking/zone detection may fail | Add stuck-sensor detection |
| Public MQTT broker | Demo-only, not secure | Use private broker with TLS/auth |
| Prototype not weatherproof | Not outdoor production-ready | Use sealed enclosure |
| No full position feedback | Robot does not know exact position | Add encoders or limit switches |
| Tailscale/backend dependency for remote dashboard | Remote access may fail | Add local interface or watchdog |

Viva answer:

> The current prototype handles many demo-level faults, but it is not production-grade. Complete hardware failures like motor failure, low battery, or a stuck-valid sensor need stronger diagnostics and redundancy. Production improvements would include encoders, battery monitoring, secure MQTT, weatherproofing, and better sensor health checks.

---

## 17. How to answer: “Do you have robustness?”

Answer:

> Yes. Robustness is one of the main criteria I can defend. The Nano remains locally autonomous if the dashboard, MQTT, Wi-Fi, or Tailscale path fails. Rain noise is handled by threshold and confirmation time. Line tracking is improved with line hunting, forward commit, and side confirmation. Zone detection uses stable counts. The backend also provides weather fallback for faulty rain telemetry.

---

## 18. How to answer: “Do you have responsiveness?”

Answer:

> Yes. The robot is responsive because the Nano checks sensors and state continuously in its loop. Rain response and line following are handled locally rather than through the dashboard. The dashboard is less time-critical and refreshes through `/api/status`.

---

## 19. How to answer: “Do you have real-time?”

Answer:

> Yes, but I would describe it as soft real-time. Rain response and line following are time-sensitive, but it is not a hard real-time system. The design prioritises local, responsive control on the Nano.

---

## 20. How to answer: “Do you have multi-threading?”

Answer:

> I would not claim multi-threading as a main feature. The Nano uses a single loop and state machine, which is appropriate for this embedded prototype. My stronger advanced criteria are robustness and responsiveness.

---

## 21. Best answer for rubric positioning

> My project satisfies the Credit criteria because it combines multiple systems, the Nano robot and Raspberry Pi backend, with MQTT communication. It satisfies Distinction-level criteria because it solves a real-world problem, reacts to environmental readings and user interaction, and was tested and optimised for fault tolerance. For the higher-level criteria, I would defend robustness and responsiveness: the Nano remains locally autonomous, sensor noise is handled with confirmation logic, and rain/line-following behaviours are handled in a soft real-time loop.

---

## 22. Final short version to memorise

```text
My advanced criteria:
  1. Robustness / fault tolerance
  2. Responsiveness / soft real-time behaviour

Do not focus on:
  - Multi-threading
```

Say:

> The system is robust because safety-critical logic is local on the Nano and sensor noise is handled using confirmation logic. It is responsive because rain response and line following are checked continuously in the Nano loop. I would describe it as a robust, soft real-time embedded IoT prototype rather than a multi-threaded system.
