# Smart Clothesline Robot Backend - MQTT Version

This backend runs on the Raspberry Pi and uses a public MQTT broker as the middleman between the Nano and the Pi.

The Nano and Pi do **not** need to be on the same Wi-Fi network anymore. They both need internet access and must use the same MQTT broker/topics.

## Structure

```text
app.py          = Flask routes
services.py     = MQTT bridge, telemetry, commands, CSV logs, weather fallback
openapi.json    = Swagger/OpenAPI spec
templates/      = dashboard and Swagger HTML
wwwroot/        = CSS and JavaScript
data/           = telemetry CSV log
```

## MQTT settings

Defaults match the Nano MQTT sketch:

```text
Broker: broker.hivemq.com
Port:   1883

Prefix: smartclothesline/manitkhera26/demo01

Telemetry topic: smartclothesline/manitkhera26/demo01/telemetry
Status topic:    smartclothesline/manitkhera26/demo01/status
Command topic:   smartclothesline/manitkhera26/demo01/command
Event topic:     smartclothesline/manitkhera26/demo01/event
```

To change the topic namespace, change `MQTT_TOPIC_PREFIX` on the backend and the matching topic strings in the Nano sketch.

## Install on Raspberry Pi / Linux

```bash
python3 -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
python3 app.py
```

Open on the same LAN:

```text
http://<raspberry-pi-ip>:5000/
http://<raspberry-pi-ip>:5000/docs
```

## Install on Windows Command Prompt

```cmd
py -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
python app.py
```

Open:

```text
http://localhost:5000/
http://localhost:5000/docs
```

## Optional MQTT environment overrides

Windows CMD:

```cmd
set MQTT_BROKER=broker.hivemq.com
set MQTT_PORT=1883
set MQTT_TOPIC_PREFIX=smartclothesline/manitkhera26/demo01
python app.py
```

Raspberry Pi / Linux:

```bash
export MQTT_BROKER="broker.hivemq.com"
export MQTT_PORT="1883"
export MQTT_TOPIC_PREFIX="smartclothesline/manitkhera26/demo01"
python3 app.py
```

## Features

- Dashboard at `/`
- Swagger at `/docs`
- MQTT telemetry subscriber
- MQTT command publisher
- CSV telemetry logging
- Event log
- Stale telemetry detection
- Melbourne weather backup using Open-Meteo
- Weather fallback can publish `MOVE_TO_SAFE` over MQTT if rain sensor data looks faulty and Melbourne weather looks bad
- Laundry Active ON/OFF command support

## Commands

Dashboard commands publish MQTT messages to the command topic.

Allowed commands:

```text
MOVE_TO_SAFE
RETURN_TO_DRYING
STOP
SET_ACTIVE_TRUE
SET_ACTIVE_FALSE
NONE
```

Payload format sent by backend:

```json
{
  "command": "MOVE_TO_SAFE",
  "source": "dashboard",
  "timestamp": "2026-05-19T10:00:00+00:00",
  "laundry_active": true
}
```

## API endpoints

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/` | Dashboard |
| GET | `/docs` | Swagger UI |
| GET | `/openapi.json` | OpenAPI JSON |
| GET | `/api/status` | Latest status from MQTT telemetry |
| GET | `/api/mqtt` | MQTT bridge status |
| POST | `/api/command` | Publish command over MQTT |
| POST | `/api/telemetry` | Legacy HTTP telemetry test endpoint |
| GET | `/api/health` | Server health |
| GET | `/api/weather` | Cached Melbourne weather |
| POST | `/api/weather/refresh` | Force weather refresh |
| GET | `/api/fallback` | Backup decision |
| GET | `/api/logs` | Recent log rows |
| GET | `/api/events` | Recent events |
| GET | `/logs.csv` | Download CSV log |

## Test MQTT from another terminal

Install a client such as `mosquitto-clients`, then:

Subscribe to telemetry:

```bash
mosquitto_sub -h broker.hivemq.com -t "smartclothesline/manitkhera26/demo01/telemetry"
```

Publish STOP:

```bash
mosquitto_pub -h broker.hivemq.com -t "smartclothesline/manitkhera26/demo01/command" -m '{"command":"STOP","source":"manual_test"}'
```

## Weather backup

Default location is Melbourne:

```text
latitude  = -37.8136
longitude = 144.9631
timezone  = Australia/Melbourne
```

Weather is considered bad if:

```text
max precipitation probability in next 3 hours >= 60%
OR expected precipitation/rain in next 3 hours > 0.2 mm
```

## Important notes

- Public MQTT brokers are for demo/testing, not production.
- Do not send private data over a public broker.
- Do not publish retained command messages, because a retained old STOP/MOVE command could trigger later.
- This backend uses non-retained command publishes.
- For remote dashboard access, use Tailscale to open `http://<pi-tailscale-ip>:5000/`.
