# Smart Clothesline Robot — Demo & Viva Flow Guide

This README is a quick reference for demonstrating and defending the Smart Clothesline Robot. It focuses on the main flows, Nano behaviour, backend/frontend flow, MQTT/Tailscale roles, and fault tolerance.

---

## 1. One-Sentence Project Summary

> My project is a Smart Clothesline Robot, which is a proof of concept for an automated clothesline that detects rain, compares sunlight, follows a line, moves between a drying zone and a safe zone, and communicates with a Raspberry Pi dashboard using MQTT.

---

## 2. Main Architecture

```text
Laptop browser
   ↓ Tailscale
Raspberry Pi Flask dashboard/backend
   ↓ MQTT
Nano 33 IoT robot
   ↓
Sensors + L298N + motors
```

Telemetry comes back the other way:

```text
Nano sensors/state
   ↓ MQTT
Raspberry Pi backend
   ↓ Flask / HTTP
Laptop dashboard
```

### Key roles

```text
Nano 33 IoT
  = robot brain / embedded controller

Raspberry Pi
  = backend, dashboard, logging, command publisher

MQTT
  = communication between Pi backend and Nano

Tailscale
  = remote access from laptop to Raspberry Pi dashboard

Laptop
  = user interface through browser
```

### Viva-safe wording

> The Nano is the safety-critical embedded controller. The Raspberry Pi is the monitoring and command interface. MQTT connects the backend and Nano. Tailscale only lets my laptop remotely access the Raspberry Pi dashboard.

---

## 3. Main Backend / Frontend Flow

The browser does **not** talk directly to the Nano.

```text
Browser dashboard
   ↓ HTTP
Flask backend endpoints
   ↓ MQTT
Nano robot
```

---

## 4. Dashboard Command Flow

This starts from the frontend.

Example: user presses **Move to Safe**.

```text
Dashboard button
   ↓
dashboard.js sendCommand("MOVE_TO_SAFE")
   ↓
POST /api/command
   ↓
Flask validates command
   ↓
Backend publishes MQTT command
   ↓
MQTT topic:
smartclothesline/manitkhera26/demo01/command
   ↓
Nano receives command
   ↓
Nano executes movement logic
```

### Supported commands

```text
MOVE_TO_SAFE
RETURN_TO_DRYING
STOP
SET_ACTIVE_TRUE
SET_ACTIVE_FALSE
```

### Say this in viva

> When I press a dashboard button, the frontend sends a POST request to `/api/command`. The Flask backend validates the command and publishes it to the MQTT command topic. The Nano subscribes to that topic and executes supported commands like `MOVE_TO_SAFE`, `RETURN_TO_DRYING`, and `STOP`.

---

## 5. Telemetry Flow

This starts from the Nano, not the frontend.

```text
Nano reads sensors/state
   ↓
Nano creates telemetry JSON
   ↓
Nano publishes to MQTT telemetry topic
   ↓
Backend receives MQTT telemetry
   ↓
Backend stores latest data
   ↓
Backend logs data
   ↓
Dashboard later reads it through /api/status
```

Telemetry topic:

```text
smartclothesline/manitkhera26/demo01/telemetry
```

Telemetry contains:

```text
state
target
event
line_pattern
rain_raw
rain_status
left_lux
right_lux
light_left_ok
light_right_ok
laundry_active
wifi_ok
mqtt_ok
last_command
```

### Important point

`/api/status` does **not** ask the Nano for fresh data directly.

It returns the latest data the backend already has.

```text
Nano → MQTT telemetry → backend memory
Frontend → GET /api/status → reads backend memory
```

### Say this in viva

> Telemetry starts from the Nano. The Nano publishes sensor and state data to MQTT. The backend receives it, stores it, logs it, and exposes it to the dashboard through `/api/status`.

---

## 6. Dashboard Refresh Flow

This starts from the frontend.

```text
dashboard.js refresh()
   ↓
GET /api/status
   ↓
Backend returns latest robot data, MQTT status, weather, fallback, and events
   ↓
Frontend updates dashboard fields
   ↓
refreshLogs()
   ↓
GET /api/logs?limit=20
   ↓
Frontend updates recent logs table
```

The dashboard normally runs this repeatedly.

```text
refresh once on page load
then refresh every 1 second
```

### Say this in viva

> The dashboard polls `/api/status` every second. It does not directly contact the Nano. It only reads the latest state stored by the backend.

---

## 7. Important Backend Endpoints

| Endpoint | Method | Purpose |
|---|---:|---|
| `/` | GET | Serves the dashboard page |
| `/api/command` | POST | Receives dashboard commands and publishes MQTT command |
| `/api/status` | GET | Main dashboard polling endpoint |
| `/api/logs` | GET | Returns recent telemetry logs |
| `/api/telemetry` | POST | Legacy/testing HTTP telemetry endpoint |
| `/api/mqtt` | GET | Shows MQTT bridge status |
| `/api/health` | GET | Backend health check |
| `/api/weather/refresh` | POST | Refreshes weather data |
| `/api/fallback` | GET | Shows fallback recommendation |
| `/logs.csv` | GET | Downloads telemetry CSV log |
| `/docs` | GET | Swagger/API docs |

### Most important endpoints to remember

```text
POST /api/command
  = sends command from dashboard to Nano through MQTT

GET /api/status
  = dashboard reads latest stored backend state
```

---

## 8. Nano Firmware Main Loop

The Nano code runs continuously.

```text
loop()
  ↓
handleSerial()
  ↓
updateRainSensor()
  ↓
updateRobot()
  ↓
updateMQTTCommunication()
  ↓
printStatus()
  ↓
repeat forever
```

### What each part does

```text
handleSerial()
  = local testing commands from Serial Monitor

updateRainSensor()
  = local rain safety logic

updateRobot()
  = state machine dispatcher

updateMQTTCommunication()
  = Wi-Fi, MQTT, command receiving, telemetry publishing

printStatus()
  = debug output
```

### Say this in viva

> The Nano firmware is structured around a continuous loop. Each loop checks local commands, checks rain safety, updates the current robot state, maintains MQTT communication, and prints debug information.

---

## 9. State vs Target

This is important.

```text
state  = what the robot is currently doing
target = where the robot is trying to go
```

Examples:

```text
state = MOVING
target = TARGET_SAFE

Meaning:
The robot is currently moving, and its goal is the safe zone.
```

```text
state = SAFE
target = TARGET_NONE

Meaning:
The robot is already in the safe zone and has no active movement target.
```

---

## 10. Main Nano States

```text
DRYING
SAFE
LINE_HUNT
MOVING
MARKER_LEAVE_FIRST
MARKER_VERIFY_SCAN
TURNING_AROUND
STOPPED
```

### State meanings

```text
DRYING
  = robot is in drying zone; can do sun tracking if laundry is active

SAFE
  = robot is in safe zone

LINE_HUNT
  = robot is searching for the line

MOVING
  = robot is following the line

MARKER_LEAVE_FIRST
  = robot has detected first 111 marker and moves off it

MARKER_VERIFY_SCAN
  = robot scans next patterns to decide SAFE or DRYING

TURNING_AROUND
  = robot performs timed turn before searching again

STOPPED
  = motors stopped by command or emergency stop
```

### State dispatcher

```text
updateRobot()
  ↓
if state == DRYING:
    updateSunTracking()

if state == LINE_HUNT:
    updateLineHunt()

if state == MOVING:
    updateMoving()

if state == MARKER_LEAVE_FIRST:
    updateMarkerLeaveFirst()

if state == MARKER_VERIFY_SCAN:
    updateMarkerVerifyScan()

if state == TURNING_AROUND:
    updateTurnAround()
```

---

## 11. DRYING to SAFE Flow

This can start from rain or from the dashboard.

### Rain-triggered path

```text
loop()
  ↓
updateRainSensor()
  ↓
rainRaw < RAIN_THRESHOLD
  ↓
rain stays wet for RAIN_CONFIRM_MS
  ↓
laundryActive == true?
  ↓
state == DRYING?
  ↓
goToTarget(TARGET_SAFE)
```

### Dashboard-triggered path

```text
Dashboard button: MOVE_TO_SAFE
  ↓
MQTT command received
  ↓
mqttCallback()
  ↓
handleMQTTCommand("MOVE_TO_SAFE")
  ↓
goToTarget(TARGET_SAFE)
```

### After `goToTarget(TARGET_SAFE)`

```text
goToTarget(TARGET_SAFE)
  ↓
stopMotors()
  ↓
if already SAFE:
    do nothing
  ↓
otherwise:
    beginLineHunt(TARGET_SAFE)
```

Then:

```text
beginLineHunt(TARGET_SAFE)
  ↓
target = TARGET_SAFE
state = LINE_HUNT
```

Then:

```text
LINE_HUNT
  ↓
find centre line pattern 010
  ↓
state = MOVING
```

Then:

```text
MOVING
  ↓
line following
  ↓
detect first wide marker 111
  ↓
beginMarkerVerification()
```

Then:

```text
MARKER_LEAVE_FIRST
  ↓
leave first 111 marker
  ↓
MARKER_VERIFY_SCAN
  ↓
look for SAFE pattern
```

SAFE pattern:

```text
111 → 000 → 111
```

If found:

```text
zoneDecision(ZONE_SAFE)
  ↓
target == TARGET_SAFE?
  ↓
yes
  ↓
state = SAFE
target = TARGET_NONE
stopMotors()
```

### Full DRYING to SAFE summary

```text
Rain confirmed OR MOVE_TO_SAFE command
  ↓
goToTarget(TARGET_SAFE)
  ↓
beginLineHunt(TARGET_SAFE)
  ↓
LINE_HUNT
  ↓
MOVING
  ↓
111 marker found
  ↓
MARKER_LEAVE_FIRST
  ↓
MARKER_VERIFY_SCAN
  ↓
detect 111 → 000 → 111
  ↓
zoneDecision(ZONE_SAFE)
  ↓
state = SAFE
target = NONE
motors stop
```

### Say this in viva

> When moving from drying to safe, the Nano sets the target to SAFE and enters line hunting. Once it finds the centre line, it moves along the track. When it detects a wide marker, it verifies the zone pattern. If it sees `111 → 000 → 111`, it confirms the safe zone, stops the motors, sets the state to SAFE, and clears the target.

---

## 12. SAFE to DRYING Flow

This starts from the dashboard command:

```text
RETURN_TO_DRYING
```

Flow:

```text
Dashboard button: RETURN_TO_DRYING
  ↓
MQTT command received
  ↓
mqttCallback()
  ↓
handleMQTTCommand("RETURN_TO_DRYING")
  ↓
goToTarget(TARGET_DRYING)
```

Important difference:

If the robot is currently in SAFE, it turns around before line hunting.

```text
goToTarget(TARGET_DRYING)
  ↓
state == SAFE and requestedTarget == TARGET_DRYING?
  ↓
yes
  ↓
beginTurnAround(TARGET_DRYING)
```

Then:

```text
beginTurnAround(TARGET_DRYING)
  ↓
state = TURNING_AROUND
pendingTargetAfterTurn = TARGET_DRYING
turnStartTime = millis()
```

Then every loop:

```text
updateRobot()
  ↓
updateTurnAround()
  ↓
rotateRight()
  ↓
after TURN_180_MS:
      stopMotors()
      beginLineHunt(TARGET_DRYING)
```

Then normal movement starts:

```text
LINE_HUNT
  ↓
find centre line 010
  ↓
MOVING
  ↓
detect first 111 marker
  ↓
MARKER_VERIFY_SCAN
  ↓
look for DRYING pattern
```

DRYING pattern:

```text
111 → 010
```

If found:

```text
zoneDecision(ZONE_DRYING)
  ↓
target == TARGET_DRYING?
  ↓
yes
  ↓
state = DRYING
target = TARGET_NONE
stopMotors()
```

### Full SAFE to DRYING summary

```text
RETURN_TO_DRYING command
  ↓
handleMQTTCommand()
  ↓
goToTarget(TARGET_DRYING)
  ↓
because state == SAFE:
      beginTurnAround(TARGET_DRYING)
  ↓
TURNING_AROUND
  ↓
after timed 180° turn:
      beginLineHunt(TARGET_DRYING)
  ↓
LINE_HUNT
  ↓
MOVING
  ↓
111 marker found
  ↓
MARKER_VERIFY_SCAN
  ↓
detect 111 → 010
  ↓
zoneDecision(ZONE_DRYING)
  ↓
state = DRYING
target = NONE
motors stop
```

### Say this in viva

> Returning from SAFE to DRYING includes an extra turn-around step. Because the robot is facing the safe direction, it first performs a timed turn, then hunts for the line again, follows it, and confirms the drying zone using the `111 → 010` pattern.

---

## 13. Line Sensor Patterns

The robot uses three line sensors.

```text
Left Centre Right
```

Patterns:

```text
100 = line on left
010 = line centred
001 = line on right
111 = wide marker
000 = no line / gap / lost line
```

---

## 14. LINE_HUNT Logic

Line hunt is used when the robot needs to find the line before moving normally.

```text
beginLineHunt(target)
  ↓
target = target
state = LINE_HUNT
huntState = HUNT_SEARCHING
reset hunt variables
```

Then:

```text
updateRobot()
  ↓
updateLineHunt()
  ↓
readLinePattern()
```

### Pattern behaviour during line hunt

```text
010
  ↓
line is centred
  ↓
state = MOVING
```

```text
001 or 011
  ↓
line is on the right
  ↓
huntState = HUNT_LOCK_RIGHT
rotateRight()
```

```text
100 or 110
  ↓
line is on the left
  ↓
huntState = HUNT_LOCK_LEFT
rotateLeft()
```

```text
111
  ↓
robot is on a marker
  ↓
creep forward
```

```text
000
  ↓
line not visible
  ↓
handleHuntNoBlack()
```

### How `handleHuntNoBlack()` helps

If the robot previously saw the line on the right, it does not instantly switch left when it sees `000`.

```text
was locked right
  ↓
now sees 000
  ↓
keep rotating right briefly
```

Why?

Because the line may just be temporarily between sensors.

If that fails:

```text
right lock failed
  ↓
switch to searching left
```

Same for left:

```text
was locked left
  ↓
now sees 000
  ↓
keep rotating left briefly
  ↓
if still not found:
      search right
```

### Say this in viva

> Line hunt uses a lock-based search. If the robot sees the line on the right, it locks right and keeps rotating right even if the line briefly disappears. This avoids rapid left-right switching. If the line is not found after a few misses or a timeout, it reverses search direction.

---

## 15. MOVING / Line Following Logic

Once the centre line is found, the robot enters `MOVING`.

```text
updateMoving()
  ↓
pattern = readLinePattern()
  ↓
handleMovingPatternWithCommit(pattern)
```

Main behaviour:

```text
010
  ↓
move forward
```

```text
100 or 110
  ↓
line is left
  ↓
confirm before correcting
  ↓
strongLeft()
```

```text
001 or 011
  ↓
line is right
  ↓
confirm before correcting
  ↓
strongRight()
```

```text
000
  ↓
line lost
  ↓
if temporary, keep forward
  ↓
otherwise beginLineHunt(target)
```

```text
111
  ↓
possible zone marker
  ↓
beginMarkerVerification()
```

---

## 16. How the Robot Stops Scribbling / Twitching

The robot can become twitchy if it reacts instantly to every sensor change. The code reduces this using four ideas.

### 1. Forward commit

When the robot sees the centre line:

```text
pattern = 010
```

it starts a short forward commit:

```text
startForwardCommit()
forward()
```

Meaning:

> For a short time after finding the centre line, trust the heading and keep moving forward.

This prevents panic from brief flickers.

Example:

```text
010 seen
  ↓
forward commit starts
  ↓
brief 100 flicker happens
  ↓
still keep moving forward
```

### 2. Side confirmation

The robot does not correct left or right from a single side reading.

Example:

```text
pattern = 100
```

Instead:

```text
sideLeftCount++
```

Only after repeated side readings:

```text
sideLeftCount >= SIDE_CONFIRM_COUNT
  ↓
strongLeft()
```

### 3. Lock-based line hunt

During line hunt, the robot does not randomly alternate left and right.

```text
line seen right
  ↓
lock right
  ↓
keep rotating right briefly
```

This prevents:

```text
right → left → right → left → right
```

### 4. Pulsed motor movement

The motors are not run at full continuous power.

```text
motor ON briefly
motor OFF briefly
motor ON briefly
motor OFF briefly
```

This makes the robot less aggressive.

### Say this in viva

> The early robot movement was twitchy because it reacted immediately to every sensor change. I reduced that using forward commit, side confirmation, lock-based line hunting, and pulsed motor movement. Forward commit makes the robot keep moving briefly after seeing the centre line. Side confirmation requires repeated side readings before correcting. Lock-based hunting prevents random left-right switching when searching for the line. Pulsed movement makes the motors less aggressive.

---

## 17. Marker Verification and Zone Detection

Marker verification starts when the robot sees:

```text
111
```

That means a wide marker may have been found.

```text
handleMovingPatternWithCommit()
  ↓
if pattern == 111:
    beginMarkerVerification()
```

Then:

```text
beginMarkerVerification()
  ↓
state = MARKER_LEAVE_FIRST
reset marker counters
```

Then:

```text
updateMarkerLeaveFirst()
  ↓
if still on 111:
    forward()
  ↓
if left first 111:
    state = MARKER_VERIFY_SCAN
```

Then:

```text
updateMarkerVerifyScan()
  ↓
if 000 stable:
    safeGapSeen = true

if 010 stable and safeGapSeen == false:
    zoneDecision(ZONE_DRYING)

if 111 stable and safeGapSeen == true:
    zoneDecision(ZONE_SAFE)

if timeout:
    zoneDecision(ZONE_UNKNOWN)
```

Zone codes:

```text
DRYING = 111 → 010
SAFE   = 111 → 000 → 111
```

### Say this in viva

> The robot does not classify a zone from one reading. It detects a first wide marker, leaves it, then scans the next stable pattern sequence. `111 → 010` means DRYING. `111 → 000 → 111` means SAFE. Stable confirmation counts reduce false zone detection.

---

## 18. Sun Tracking Flow

Sun tracking only runs when:

```text
state == DRYING
laundryActive == true
leftLightOK == true
rightLightOK == true
```

Flow:

```text
updateSunTracking()
  ↓
read leftLux
read rightLux
  ↓
diff = leftLux - rightLux
  ↓
if difference is inside deadband:
    stopMotors()
    setEvent("SUN_BALANCED")

if left is brighter by more than deadband:
    rotate left briefly

if right is brighter by more than deadband:
    rotate right briefly
```

### What is deadband?

A deadband is a small tolerance range where the robot deliberately does nothing.

Example:

```text
LIGHT_DEADBAND = 20 lux

Left = 105 lux
Right = 100 lux
Difference = 5 lux

5 lux is inside the deadband
→ do not rotate
```

Example where it does rotate:

```text
LIGHT_DEADBAND = 20 lux

Left = 150 lux
Right = 100 lux
Difference = 50 lux

50 lux is outside the deadband
→ rotate left
```

### Say this in viva

> The deadband prevents tiny BH1750 fluctuations from causing constant rotation. If the lux difference is small, the robot treats the light as balanced. If one side is clearly brighter, it rotates toward that side.

---

## 19. Fault Tolerances Currently Handled

| Fault/problem | Current tolerance level | How it handles it | Where handled |
|---|---:|---|---|
| Dashboard/laptop fails | Strong | Robot does not depend on dashboard for safety. Nano keeps running its own loop. | Nano firmware / architecture |
| Tailscale fails | Strong for robot, weak for remote access | Only remote browser access is lost. Nano does not use Tailscale. | Architecture |
| Wi-Fi fails on Nano | Partial | MQTT telemetry and commands stop, but local sensor/motor logic continues. | Nano local autonomy |
| MQTT broker/connection fails | Partial | Backend cannot send commands or receive telemetry, but Nano still handles local rain and movement logic. | Nano firmware / architecture |
| Rain sensor noise | Strong | Uses analog threshold plus confirmation time. One bad wet reading does not trigger movement. | `updateRainSensor()` |
| Obvious rain sensor fault | Good / partial | If telemetry shows missing/invalid/unknown rain data, backend checks weather and can publish `MOVE_TO_SAFE`. | Backend fallback logic |
| Line sensor flicker | Strong | Uses forward commit and side confirmation to avoid twitchy corrections. | `updateMoving()` |
| Temporary line loss | Strong | Uses line hunting and last known correction direction to search again. | `LINE_HUNT` / `updateLineHunt()` |
| False zone detection | Strong | Uses repeated stable pattern counts before confirming SAFE or DRYING. | Marker verification |
| Tiny BH1750 light differences | Moderate | Uses light deadband to avoid unnecessary rotation. | `updateSunTracking()` |
| Flask backend crash / Pi reboot | Moderate | Backend runs as a `systemd` service and can restart/start on boot. | Raspberry Pi deployment |
| Conflicting movement commands | Moderate | Movement commands can be ignored during movement-critical states. `STOP` remains the key override. | MQTT command handling |
| Telemetry stale | Moderate | Dashboard/backend can show stale data instead of pretending old data is live. | Backend status payload |

---

## 20. Faults Not Fully Handled / Future Improvements

| Fault/problem | Current limitation | Improvement answer |
|---|---|---|
| Rain sensor stuck at believable dry value | Backend may think the value is healthy. | Add sensor diagnostics, compare with weather over time, or add redundant rain sensor. |
| Backend weather fallback unavailable | If backend, MQTT, Wi-Fi, or weather API fails, fallback command cannot reach Nano. | Use local broker, stronger reconnect logic, cached weather, or secondary communication path. |
| BH1750 complete failure | Sun tracking becomes unreliable, but it is not safety-critical. | Disable sun tracking if sensor init fails and show dashboard warning. |
| Motor failure | Nano commands motors but cannot verify actual movement. | Add wheel encoders, current sensing, or position feedback. |
| Low battery | Weak motors, resets, and bad readings may happen. | Add battery voltage monitoring and dashboard warning. |
| Permanent line sensor failure | Code handles flicker, not a permanently dead sensor. | Add stuck-sensor detection, calibration checks, or extra sensors. |
| Track damaged / robot far from line | Line hunting assumes the line can be found nearby. | Add timeout, better physical guide, encoders, or manual recovery mode. |
| Public MQTT security | Public broker is demo-only. | Use private broker with TLS, authentication, and access control. |
| Pi/Nano power failure | Device stops if it loses power. | Add UPS, battery monitoring, and safe shutdown behaviour. |
| Weatherproofing | Prototype is not production outdoor-rated. | Use sealed enclosure, waterproof connectors, outdoor-rated sensors. |

### Fault-tolerance viva summary

> The system handles many realistic demo faults through layered fault tolerance. The Nano remains locally autonomous if the dashboard, Tailscale, Wi-Fi, or MQTT path fails. Sensor noise is handled through confirmation logic: rain uses threshold plus time confirmation, line following uses forward commit and side confirmation, zone detection uses repeated stable marker counts, and sun tracking uses a deadband. The backend also provides a weather fallback if rain telemetry looks faulty. The weaker areas are complete hardware failures like motors, battery, or a sensor stuck at a believable value; those would need encoders, battery monitoring, redundant sensors, and stronger diagnostics in a production version.

---

## 21. MQTT Not Connected — Quick Demo Troubleshooting

If dashboard shows:

```text
MQTT Connected: NO
```

It means the Raspberry Pi backend is not connected to the MQTT broker.

### Step 1: Check backend MQTT status

On Raspberry Pi:

```bash
curl http://localhost:5000/api/mqtt
```

Look for:

```text
started
connected
last_error
```

### Step 2: Try safe dummy command

This may start the MQTT bridge if it has not started.

```bash
curl -X POST http://localhost:5000/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"NONE"}'
```

Then refresh dashboard.

### Step 3: Restart backend service

```bash
sudo systemctl restart smart-clothesline.service
sudo systemctl status smart-clothesline.service
```

### Step 4: Check logs

```bash
journalctl -u smart-clothesline.service -n 50
```

or live:

```bash
journalctl -u smart-clothesline.service -f
```

### Step 5: Check broker connection

```bash
ping broker.hivemq.com
```

```bash
nc -vz broker.hivemq.com 1883
```

### Viva explanation if MQTT is down

> MQTT failure affects remote telemetry and manual commands, but it does not stop the Nano’s local safety logic. The robot can still read the rain sensor and execute its local state machine.

---

## 22. Best Demo Order

Use this order if possible:

```text
1. Show dashboard and architecture
2. Show telemetry/status values
3. Press SET_ACTIVE_TRUE
4. Demo sun/light comparison if practical
5. Press MOVE_TO_SAFE
6. Explain DRYING → SAFE flow
7. Press RETURN_TO_DRYING
8. Explain SAFE → DRYING flow
9. Wet rain sensor and show local rain safety
10. Explain backend weather fallback if rain sensor telemetry is faulty
11. Explain fault tolerance and limitations
```

If something fails:

```text
Do not panic.
Explain which layer failed.
Then explain what still works locally on the Nano.
```

---

## 23. Most Important Things to Say

### Architecture

> The Nano is the robot brain. The Raspberry Pi is the dashboard/backend. MQTT connects the backend and Nano. Tailscale only gives laptop access to the Pi dashboard.

### Local autonomy

> Safety-critical logic is local on the Nano. Dashboard, Tailscale, or MQTT failure affects monitoring and remote commands, not the local rain safety loop.

### Command flow

> Button press goes to `/api/command`, Flask publishes MQTT command, Nano receives it and executes it.

### Telemetry flow

> Nano publishes telemetry to MQTT. Backend stores/logs it. Dashboard reads it through `/api/status`.

### Movement flow

> Commands and rain set a target. The state machine handles the movement through `LINE_HUNT`, `MOVING`, marker verification, and zone decision.

### Rain safety

> Rain uses threshold plus confirmation time. If rain is confirmed while laundry is active and the robot is in DRYING, the Nano calls `goToTarget(TARGET_SAFE)`.

### Line tracking

> The robot follows three line sensor patterns. It uses forward commit and side confirmation to stop twitching.

### Zone detection

> DRYING is `111 → 010`. SAFE is `111 → 000 → 111`. Stable counts prevent false detection.

### Fault tolerance

> The strongest fault tolerance is local autonomy on the Nano plus confirmation logic for noisy sensors. The weaker areas are complete hardware failure, low battery, and production security.

---

## 24. One-Minute Final Viva Summary

> This project is a proof-of-concept Smart Clothesline Robot. The Nano 33 IoT acts as the embedded controller: it reads the rain sensor, line sensors, and BH1750 light sensors, then controls the motors through the L298N. The Raspberry Pi runs a Flask dashboard that receives telemetry and sends manual commands through MQTT. The laptop accesses the dashboard through Tailscale, but the Nano does not use Tailscale. The robot is locally autonomous, so rain safety, line tracking, zone detection, sun tracking, and motor control happen on the Nano. Commands and rain events set a target such as SAFE or DRYING, and the Nano state machine handles the movement through line hunting, moving, marker verification, and zone decision. Fault tolerance is handled through local autonomy, rain confirmation, line recovery, stable zone detection, light deadband, backend weather fallback, and systemd backend deployment.
