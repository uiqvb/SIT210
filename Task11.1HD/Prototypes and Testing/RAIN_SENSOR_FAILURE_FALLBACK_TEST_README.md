# Rain Sensor Failure / Weather Fallback Test Guide

This README explains how to test the **rain sensor failure fallback** for the Smart Clothesline Robot backend running on the Raspberry Pi.

The purpose of this test is to prove that if rain telemetry looks faulty or unavailable, the backend can use weather data as a secondary fallback and recommend or publish `MOVE_TO_SAFE`.

---

## 1. What this test demonstrates

The normal rain safety path is local:

```text
Nano rain sensor detects rain
  ↓
Nano confirms rain locally
  ↓
Nano moves robot to SAFE
```

The fallback path is backend-assisted:

```text
Backend receives faulty/unknown rain telemetry
  ↓
Backend checks local weather data
  ↓
Weather says rain is likely
  ↓
Backend recommends or publishes MOVE_TO_SAFE
```

### Important viva explanation

> The local rain sensor is the primary safety mechanism. The backend weather fallback is secondary. It helps when rain telemetry looks faulty, missing, or unknown, but it depends on the backend, MQTT, and weather data being available.

---

## 2. When to run these commands

Run these commands **on the Raspberry Pi terminal** because Flask is running on the Pi.

If you are SSHed into the Pi, use:

```bash
ssh <pi-user>@<pi-ip-or-tailscale-ip>
```

Then run the commands below.

---

## 3. Step 1 — Confirm Flask backend is running

```bash
curl http://localhost:5000/api/health
```

Expected result:

```text
A JSON response showing backend/server health.
```

If this fails, restart the backend service:

```bash
sudo systemctl restart smart-clothesline.service
sudo systemctl status smart-clothesline.service
```

Check logs:

```bash
journalctl -u smart-clothesline.service -n 50
```

---

## 4. Step 2 — Check MQTT bridge status

```bash
curl http://localhost:5000/api/mqtt
```

Look for fields such as:

```text
connected
started
broker
last_error
```

If MQTT is not connected, try a safe dummy command:

```bash
curl -X POST http://localhost:5000/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"NONE"}'
```

Then recheck:

```bash
curl http://localhost:5000/api/mqtt
```

If still not connected, restart the backend:

```bash
sudo systemctl restart smart-clothesline.service
```

Then check:

```bash
journalctl -u smart-clothesline.service -f
```

---

## 5. Step 3 — Refresh weather data

The fallback depends on weather data, so refresh it first.

```bash
curl -X POST http://localhost:5000/api/weather/refresh
```

Then check fallback/weather status:

```bash
curl http://localhost:5000/api/fallback
```

You want to see that the weather suggests rain or bad weather.

Look for ideas like:

```text
weather_rain_likely: true
rain likely: true
weather_bad: true
```

The exact field names may differ depending on the backend response.

---

## 6. Step 4 — Safe test using HTTP telemetry

This is the safest way to demo the fallback.

It uses the backend's legacy/testing endpoint:

```text
POST /api/telemetry
```

This does not require the Nano to actually send faulty data. It injects simulated faulty telemetry into the backend.

### Run this command on the Pi

```bash
curl -X POST http://localhost:5000/api/telemetry \
  -H "Content-Type: application/json" \
  -d '{
    "device": "nano",
    "state": "DRYING",
    "target": "NONE",
    "event": "DEMO_FAULTY_RAIN_SENSOR",
    "line_pattern": "010",
    "rain_status": "UNKNOWN",
    "left_lux": 120,
    "right_lux": 100,
    "light_left_ok": true,
    "light_right_ok": true,
    "laundry_active": true,
    "wifi_ok": true,
    "mqtt_ok": true,
    "last_command": "NONE"
  }'
```

### Why this works

This payload intentionally does **not** include:

```text
rain_raw
```

and it sets:

```text
rain_status = UNKNOWN
```

That makes the backend treat the rain telemetry as faulty, missing, or unreliable.

---

## 7. Step 5 — Check fallback decision

After sending the fake faulty telemetry, run:

```bash
curl http://localhost:5000/api/fallback
```

Also check the full dashboard status payload:

```bash
curl http://localhost:5000/api/status
```

Look for signs like:

```text
RAIN_SENSOR_FAULT
RAIN_RAW_MISSING
RAIN_STATUS_UNKNOWN
RAIN_SENSOR_FAULT_WEATHER_BAD
recommended_command: MOVE_TO_SAFE
command: MOVE_TO_SAFE
```

The exact field names may differ, but the concept should be:

```text
rain sensor telemetry is faulty
weather says rain is likely
backend recommends MOVE_TO_SAFE
```

---

## 8. Step 6 — Watch backend logs during the test

Open another terminal on the Pi and run:

```bash
journalctl -u smart-clothesline.service -f
```

Then run the fake telemetry command again.

This lets you show backend activity live during the demo.

---

## 9. Optional full MQTT fallback test

Use this only if the robot is safe to move.

This simulates faulty telemetry arriving through MQTT instead of HTTP.

### Requirements before running this

Make sure:

```text
Robot is on the track
Path to SAFE is clear
Robot is not already moving
Laundry active is true
MQTT bridge is connected
You are ready for the robot to move
```

### Publish fake faulty telemetry to MQTT

```bash
mosquitto_pub -h broker.hivemq.com -p 1883 \
  -t smartclothesline/manitkhera26/demo01/telemetry \
  -m '{
    "device": "nano",
    "state": "DRYING",
    "target": "NONE",
    "event": "DEMO_MQTT_FAULTY_RAIN_SENSOR",
    "line_pattern": "010",
    "rain_status": "UNKNOWN",
    "left_lux": 120,
    "right_lux": 100,
    "light_left_ok": true,
    "light_right_ok": true,
    "laundry_active": true,
    "wifi_ok": true,
    "mqtt_ok": true,
    "last_command": "NONE"
  }'
```

Again, this intentionally omits `rain_raw`.

Expected backend behaviour:

```text
Backend receives faulty telemetry on MQTT
  ↓
Backend checks weather fallback
  ↓
If weather says rain likely:
    backend publishes MOVE_TO_SAFE
  ↓
Nano may receive MOVE_TO_SAFE and move to SAFE
```

### Warning

Only use the MQTT version when the robot is physically safe to move.

---

## 10. Safer demo script

Use this version during the viva if you do not want the robot to unexpectedly move.

```text
1. Show the dashboard.
2. Press Refresh Weather or run /api/weather/refresh.
3. Explain that today's weather is rainy.
4. Run the HTTP /api/telemetry curl command with rain_status UNKNOWN and no rain_raw.
5. Show /api/fallback or /api/status.
6. Point out the backend fallback decision.
7. Explain that in the live MQTT path, the backend can publish MOVE_TO_SAFE.
```

### Say this

> I am injecting simulated faulty telemetry using the testing endpoint. In normal operation, telemetry arrives through MQTT from the Nano. Here I am proving the fallback decision safely: the backend sees rain telemetry as faulty, checks weather, and because rain is likely, it recommends `MOVE_TO_SAFE`.

---

## 11. Common problems and fixes

### Problem: `curl http://localhost:5000/api/health` fails

Flask is not running or not on port 5000.

Fix:

```bash
sudo systemctl restart smart-clothesline.service
sudo systemctl status smart-clothesline.service
```

Logs:

```bash
journalctl -u smart-clothesline.service -n 50
```

---

### Problem: MQTT connected is NO

Try:

```bash
curl -X POST http://localhost:5000/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"NONE"}'
```

Then:

```bash
curl http://localhost:5000/api/mqtt
```

If still not connected:

```bash
sudo systemctl restart smart-clothesline.service
journalctl -u smart-clothesline.service -f
```

Check broker access:

```bash
ping broker.hivemq.com
```

```bash
nc -vz broker.hivemq.com 1883
```

---

### Problem: fallback says command is NONE

Possible reasons:

```text
Weather does not currently say rain likely
Telemetry does not look faulty enough
Robot state is not DRYING
Target is already SAFE
Laundry active is false
Backend fallback rules are preventing repeat command
MQTT/backend is not connected
```

Try sending the test payload again with:

```text
state = DRYING
target = NONE
rain_status = UNKNOWN
laundry_active = true
rain_raw omitted
```

---

### Problem: local rain sensor overrides the fallback

This is normal.

The local rain sensor is primary. If the Nano itself detects real rain, it may move to SAFE through local logic before the backend fallback matters.

Viva wording:

> The weather fallback is not meant to replace local rain detection. It is a secondary fallback when rain telemetry looks faulty.

---

## 12. Best viva answer

> The rain safety system has two layers. The primary layer is local on the Nano: it reads the rain sensor, confirms a wet reading over time, and moves to SAFE if laundry is active and the robot is in DRYING. The secondary layer is the backend weather fallback. If the backend receives telemetry where rain data is missing, invalid, or unknown, it can check weather data and recommend or publish `MOVE_TO_SAFE`. This improves fault tolerance, but it still depends on backend, MQTT, and weather availability, so it is not a full replacement for the local rain sensor.

---

## 13. Quick command list

Health:

```bash
curl http://localhost:5000/api/health
```

MQTT status:

```bash
curl http://localhost:5000/api/mqtt
```

Start MQTT with dummy command:

```bash
curl -X POST http://localhost:5000/api/command \
  -H "Content-Type: application/json" \
  -d '{"command":"NONE"}'
```

Refresh weather:

```bash
curl -X POST http://localhost:5000/api/weather/refresh
```

Check fallback:

```bash
curl http://localhost:5000/api/fallback
```

Inject faulty rain telemetry:

```bash
curl -X POST http://localhost:5000/api/telemetry \
  -H "Content-Type: application/json" \
  -d '{
    "device": "nano",
    "state": "DRYING",
    "target": "NONE",
    "event": "DEMO_FAULTY_RAIN_SENSOR",
    "line_pattern": "010",
    "rain_status": "UNKNOWN",
    "left_lux": 120,
    "right_lux": 100,
    "light_left_ok": true,
    "light_right_ok": true,
    "laundry_active": true,
    "wifi_ok": true,
    "mqtt_ok": true,
    "last_command": "NONE"
  }'
```

Check dashboard status:

```bash
curl http://localhost:5000/api/status
```

Watch service logs:

```bash
journalctl -u smart-clothesline.service -f
```
