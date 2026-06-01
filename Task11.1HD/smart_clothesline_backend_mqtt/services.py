from __future__ import annotations

import csv
import json
import os
import threading
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional

import requests

try:
    import paho.mqtt.client as mqtt
except Exception:
    mqtt = None


class Config:
    HOST = os.environ.get("HOST", "0.0.0.0")
    PORT = int(os.environ.get("PORT", "5000"))
    DEBUG = os.environ.get("DEBUG", "false").lower() == "true"

    DATA_DIR = Path(os.environ.get("DATA_DIR", "data"))
    CSV_PATH = DATA_DIR / "telemetry_log.csv"
    WEATHER_CONFIG_PATH = DATA_DIR / "weather_config.json"
    OPENAPI_PATH = Path(os.environ.get("OPENAPI_PATH", "openapi.json"))

    STALE_AFTER_SECONDS = float(os.environ.get("STALE_AFTER_SECONDS", "10"))

    # MQTT defaults must match the Nano sketch.
    MQTT_ENABLED = os.environ.get("MQTT_ENABLED", "true").lower() == "true"
    MQTT_BROKER = os.environ.get("MQTT_BROKER", "broker.hivemq.com")
    MQTT_PORT = int(os.environ.get("MQTT_PORT", "1883"))
    MQTT_KEEPALIVE = int(os.environ.get("MQTT_KEEPALIVE", "60"))

    MQTT_USERNAME = os.environ.get("MQTT_USERNAME", "")
    MQTT_PASSWORD = os.environ.get("MQTT_PASSWORD", "")

    MQTT_TOPIC_PREFIX = os.environ.get("MQTT_TOPIC_PREFIX", "smartclothesline/manitkhera26/demo01")
    MQTT_TELEMETRY_TOPIC = os.environ.get("MQTT_TELEMETRY_TOPIC", f"{MQTT_TOPIC_PREFIX}/telemetry")
    MQTT_STATUS_TOPIC = os.environ.get("MQTT_STATUS_TOPIC", f"{MQTT_TOPIC_PREFIX}/status")
    MQTT_COMMAND_TOPIC = os.environ.get("MQTT_COMMAND_TOPIC", f"{MQTT_TOPIC_PREFIX}/command")
    MQTT_EVENT_TOPIC = os.environ.get("MQTT_EVENT_TOPIC", f"{MQTT_TOPIC_PREFIX}/event")

    MQTT_CLIENT_ID = os.environ.get("MQTT_CLIENT_ID", "smart-clothesline-pi-backend")

    # Melbourne defaults.
    WEATHER_LAT = os.environ.get("WEATHER_LAT", "-37.8136")
    WEATHER_LON = os.environ.get("WEATHER_LON", "144.9631")
    WEATHER_LOCATION_NAME = os.environ.get("WEATHER_LOCATION_NAME", "Melbourne")
    WEATHER_TIMEZONE = os.environ.get("WEATHER_TIMEZONE", "Australia/Melbourne")

    WEATHER_RAIN_PROB_THRESHOLD = int(os.environ.get("WEATHER_RAIN_PROB_THRESHOLD", "60"))
    WEATHER_PRECIP_MM_THRESHOLD = float(os.environ.get("WEATHER_PRECIP_MM_THRESHOLD", "0.2"))
    WEATHER_LOOKAHEAD_HOURS = int(os.environ.get("WEATHER_LOOKAHEAD_HOURS", "3"))
    WEATHER_CACHE_SECONDS = int(os.environ.get("WEATHER_CACHE_SECONDS", "600"))

    FALLBACK_ENABLED = os.environ.get("FALLBACK_ENABLED", "true").lower() == "true"

    ALLOWED_COMMANDS = {
        "NONE",
        "MOVE_TO_SAFE",
        "RETURN_TO_DRYING",
        "STOP",
        "SET_ACTIVE_TRUE",
        "SET_ACTIVE_FALSE",
    }

    @classmethod
    def ensure_directories(cls) -> None:
        cls.DATA_DIR.mkdir(parents=True, exist_ok=True)


class RuntimeState:
    def __init__(self) -> None:
        self.lock = threading.RLock()

        self.latest_data: Dict[str, Any] = {}
        self.last_update_time: Optional[datetime] = None

        # For MQTT mode this means "last command requested", not an HTTP command waiting to be consumed.
        self.pending_command = "NONE"
        self.last_command_published = "NONE"
        self.last_command_source = "NONE"
        self.last_command_time: Optional[str] = None

        self.laundry_active = True

        self.event_log: List[Dict[str, Any]] = []
        self.last_logged_event: Optional[str] = None

        self.weather_cache: Dict[str, Any] = {"updated_at": None, "data": None, "error": None}
        self.last_rain_raw: Optional[int] = None
        self.last_rain_change_time: Optional[datetime] = None
        # Tracks the first moment a sensor fault was detected in the current fault window.
        # Reset to None when the sensor recovers, so the next fault triggers a fresh weather fetch.
        self.last_sensor_fault_time: Optional[datetime] = None

        self.mqtt_connected = False
        self.mqtt_started = False
        self.mqtt_last_connect_time: Optional[str] = None
        self.mqtt_last_message_time: Optional[str] = None
        self.mqtt_last_publish_time: Optional[str] = None
        self.mqtt_last_error: Optional[str] = None


runtime = RuntimeState()
_mqtt_client = None
_mqtt_lock = threading.RLock()


COMMAND_LABELS = {
    "MOVE_TO_SAFE": "Move to Safe",
    "RETURN_TO_DRYING": "Return to Drying",
    "STOP": "Stop",
    "SET_ACTIVE_TRUE": "Laundry Active ON",
    "SET_ACTIVE_FALSE": "Laundry Active OFF",
    "NONE": "None",
}


CSV_FIELDNAMES = [
    "timestamp",
    "source",
    "state",
    "target",
    "event",
    "line_pattern",
    "rain_raw",
    "rain_status",
    "left_lux",
    "right_lux",
    "light_mode",
    "line_lost_fault",
    "movement_timeout_fault",
    "light_tracking_timeout_fault",
    "light_left_ok",
    "light_right_ok",
    "laundry_active",
    "command_sent",
    "command_source",
    "fallback_active",
    "fallback_reason",
    "weather_rain_likely",
    "data_status",
    "ip",
]


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def iso_now() -> str:
    return utc_now().isoformat(timespec="seconds")


def age_seconds(value: Optional[datetime]) -> Optional[float]:
    if value is None:
        return None
    return round((utc_now() - value).total_seconds(), 2)


def command_label(command: str) -> str:
    return COMMAND_LABELS.get(command, command)


def get_data_status() -> str:
    age = age_seconds(runtime.last_update_time)
    if age is None:
        return "NO_DATA"
    if age > Config.STALE_AFTER_SECONDS:
        return "STALE"
    return "LIVE"


def add_event(event: str, state: str = "", target: str = "", extra: Optional[Dict[str, Any]] = None) -> None:
    if not event or event == "NONE":
        return

    compact_key = f"{event}|{state}|{target}|{json.dumps(extra, sort_keys=True) if extra else ''}"
    if compact_key == runtime.last_logged_event:
        return

    runtime.last_logged_event = compact_key

    entry = {
        "timestamp": iso_now(),
        "event": event,
        "state": state,
        "target": target,
    }

    if extra:
        entry.update(extra)

    runtime.event_log.insert(0, entry)
    del runtime.event_log[200:]


def get_events() -> List[Dict[str, Any]]:
    with runtime.lock:
        return list(runtime.event_log)


# ----------------------------
# MQTT BRIDGE
# ----------------------------

def start_mqtt_bridge() -> None:
    global _mqtt_client

    if not Config.MQTT_ENABLED:
        with runtime.lock:
            runtime.mqtt_last_error = "MQTT disabled"
        return

    if mqtt is None:
        with runtime.lock:
            runtime.mqtt_last_error = "paho-mqtt not installed"
        return

    with _mqtt_lock:
        if runtime.mqtt_started:
            return

        try:
            # paho-mqtt 2.x supports CallbackAPIVersion. Older versions do not.
            try:
                client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id=Config.MQTT_CLIENT_ID, clean_session=True)
            except Exception:
                client = mqtt.Client(client_id=Config.MQTT_CLIENT_ID, clean_session=True)

            if Config.MQTT_USERNAME:
                client.username_pw_set(Config.MQTT_USERNAME, Config.MQTT_PASSWORD)

            client.on_connect = _on_mqtt_connect
            client.on_disconnect = _on_mqtt_disconnect
            client.on_message = _on_mqtt_message

            client.connect_async(Config.MQTT_BROKER, Config.MQTT_PORT, Config.MQTT_KEEPALIVE)
            client.loop_start()

            _mqtt_client = client

            with runtime.lock:
                runtime.mqtt_started = True
                runtime.mqtt_last_error = None

        except Exception as exc:
            with runtime.lock:
                runtime.mqtt_started = False
                runtime.mqtt_connected = False
                runtime.mqtt_last_error = str(exc)


def _on_mqtt_connect(client, userdata, flags, rc):
    with runtime.lock:
        runtime.mqtt_connected = rc == 0
        runtime.mqtt_last_connect_time = iso_now()
        runtime.mqtt_last_error = None if rc == 0 else f"MQTT connect failed rc={rc}"

    if rc == 0:
        client.subscribe(Config.MQTT_TELEMETRY_TOPIC)
        client.subscribe(Config.MQTT_STATUS_TOPIC)
        add_event("MQTT_CONNECTED", extra={"broker": Config.MQTT_BROKER})

    else:
        add_event("MQTT_CONNECT_FAILED", extra={"rc": rc})


def _on_mqtt_disconnect(client, userdata, rc):
    with runtime.lock:
        runtime.mqtt_connected = False
        runtime.mqtt_last_error = None if rc == 0 else f"MQTT disconnected rc={rc}"
    add_event("MQTT_DISCONNECTED", extra={"rc": rc})


def _on_mqtt_message(client, userdata, msg):
    try:
        payload_text = msg.payload.decode("utf-8", errors="replace")
        with runtime.lock:
            runtime.mqtt_last_message_time = iso_now()

        if msg.topic == Config.MQTT_TELEMETRY_TOPIC:
            payload = json.loads(payload_text)
            if isinstance(payload, dict):
                process_telemetry(payload, remote_ip="mqtt", source="mqtt")
            return

        if msg.topic == Config.MQTT_STATUS_TOPIC:
            payload = {}
            try:
                payload = json.loads(payload_text)
            except Exception:
                payload = {"raw": payload_text}

            status_name = str(payload.get("status", "MQTT_STATUS"))
            add_event(
                status_name,
                state=str(payload.get("state", "")),
                target=str(payload.get("target", "")),
                extra={"source": "mqtt_status", "payload": payload},
            )
            return

    except Exception as exc:
        with runtime.lock:
            runtime.mqtt_last_error = f"MQTT message error: {exc}"
        add_event("MQTT_MESSAGE_ERROR", extra={"error": str(exc), "topic": getattr(msg, "topic", "")})


def publish_mqtt_command(command: str, source: str = "backend", reason: Optional[str] = None) -> Dict[str, Any]:
    start_mqtt_bridge()

    cmd = command.upper().strip()

    if cmd not in Config.ALLOWED_COMMANDS:
        return {
            "ok": False,
            "error": f"Invalid command: {cmd}",
            "allowed": sorted(Config.ALLOWED_COMMANDS),
        }

    with runtime.lock:
        if cmd == "SET_ACTIVE_TRUE":
            runtime.laundry_active = True
        elif cmd == "SET_ACTIVE_FALSE":
            runtime.laundry_active = False

    payload = {
        "command": cmd,
        "source": source,
        "timestamp": iso_now(),
        "laundry_active": runtime.laundry_active,
    }

    if reason:
        payload["reason"] = reason

    payload_text = json.dumps(payload, separators=(",", ":"))

    with _mqtt_lock:
        client = _mqtt_client

        if client is None or not client.is_connected():
            with runtime.lock:
                runtime.pending_command = cmd
                runtime.last_command_published = cmd
                runtime.last_command_source = source
                runtime.last_command_time = iso_now()
                runtime.mqtt_last_error = "MQTT not connected; command could not be published"

            add_event(
                f"MQTT_COMMAND_FAILED_{cmd}",
                state=str(runtime.latest_data.get("state", "")),
                target=str(runtime.latest_data.get("target", "")),
                extra={"source": source, "reason": "MQTT_NOT_CONNECTED"},
            )

            return {
                "ok": False,
                "error": "MQTT not connected; command was not published",
                "command": cmd,
                "mqtt": build_mqtt_payload(),
            }

        info = client.publish(Config.MQTT_COMMAND_TOPIC, payload_text, qos=0, retain=False)

    success = getattr(info, "rc", 1) == 0

    with runtime.lock:
        runtime.pending_command = cmd
        runtime.last_command_published = cmd
        runtime.last_command_source = source
        runtime.last_command_time = iso_now()
        runtime.mqtt_last_publish_time = runtime.last_command_time
        runtime.mqtt_last_error = None if success else f"MQTT publish failed rc={getattr(info, 'rc', 'unknown')}"

    if success:
        add_event(
            f"MQTT_COMMAND_PUBLISHED_{cmd}",
            state=str(runtime.latest_data.get("state", "")),
            target=str(runtime.latest_data.get("target", "")),
            extra={"source": source, "reason": reason},
        )

        return {
            "ok": True,
            "command": cmd,
            "topic": Config.MQTT_COMMAND_TOPIC,
            "payload": payload,
            "mqtt": build_mqtt_payload(),
        }

    add_event(
        f"MQTT_COMMAND_FAILED_{cmd}",
        state=str(runtime.latest_data.get("state", "")),
        target=str(runtime.latest_data.get("target", "")),
        extra={"source": source, "reason": reason, "publish_rc": getattr(info, "rc", None)},
    )

    return {
        "ok": False,
        "error": f"MQTT publish failed rc={getattr(info, 'rc', 'unknown')}",
        "command": cmd,
        "mqtt": build_mqtt_payload(),
    }


def publish_manual_command(command: str) -> Dict[str, Any]:
    return publish_mqtt_command(command, source="dashboard")


def build_mqtt_payload() -> Dict[str, Any]:
    with runtime.lock:
        return {
            "enabled": Config.MQTT_ENABLED,
            "connected": runtime.mqtt_connected,
            "started": runtime.mqtt_started,
            "broker": Config.MQTT_BROKER,
            "port": Config.MQTT_PORT,
            "topic_prefix": Config.MQTT_TOPIC_PREFIX,
            "telemetry_topic": Config.MQTT_TELEMETRY_TOPIC,
            "status_topic": Config.MQTT_STATUS_TOPIC,
            "command_topic": Config.MQTT_COMMAND_TOPIC,
            "last_connect_time": runtime.mqtt_last_connect_time,
            "last_message_time": runtime.mqtt_last_message_time,
            "last_publish_time": runtime.mqtt_last_publish_time,
            "last_error": runtime.mqtt_last_error,
            "last_command_published": runtime.last_command_published,
            "last_command_source": runtime.last_command_source,
            "last_command_time": runtime.last_command_time,
        }


# ----------------------------
# WEATHER + FALLBACK
# ----------------------------

def _safe_float(value: Any, field_name: str, min_value: float, max_value: float) -> float:
    try:
        result = float(value)
    except (TypeError, ValueError):
        raise ValueError(f"{field_name} must be a number")

    if result < min_value or result > max_value:
        raise ValueError(f"{field_name} must be between {min_value} and {max_value}")

    return result


def _apply_weather_config(data: Dict[str, Any]) -> None:
    Config.WEATHER_LOCATION_NAME = str(data.get("location_name", Config.WEATHER_LOCATION_NAME)).strip() or "Configured location"
    Config.WEATHER_LAT = str(data.get("latitude", Config.WEATHER_LAT)).strip()
    Config.WEATHER_LON = str(data.get("longitude", Config.WEATHER_LON)).strip()
    Config.WEATHER_TIMEZONE = str(data.get("timezone", Config.WEATHER_TIMEZONE)).strip() or "auto"


def load_weather_config() -> None:
    try:
        if Config.WEATHER_CONFIG_PATH.exists():
            data = json.loads(Config.WEATHER_CONFIG_PATH.read_text(encoding="utf-8"))
            if isinstance(data, dict):
                _apply_weather_config(data)
    except Exception as exc:
        with runtime.lock:
            runtime.weather_cache["error"] = f"Weather config load failed: {exc}"


def weather_configured() -> bool:
    load_weather_config()
    return bool(Config.WEATHER_LAT and Config.WEATHER_LON)


def get_weather_config() -> Dict[str, Any]:
    load_weather_config()
    return {
        "location_name": Config.WEATHER_LOCATION_NAME,
        "latitude": Config.WEATHER_LAT,
        "longitude": Config.WEATHER_LON,
        "timezone": Config.WEATHER_TIMEZONE,
        "rain_probability_threshold_percent": Config.WEATHER_RAIN_PROB_THRESHOLD,
        "precip_threshold_mm": Config.WEATHER_PRECIP_MM_THRESHOLD,
        "lookahead_hours": Config.WEATHER_LOOKAHEAD_HOURS,
        "config_path": str(Config.WEATHER_CONFIG_PATH),
    }


def update_weather_config(payload: Dict[str, Any]) -> Dict[str, Any]:
    if not isinstance(payload, dict):
        return {"ok": False, "error": "Expected JSON object"}

    try:
        location_name = str(payload.get("location_name", "")).strip() or "Configured location"
        latitude = _safe_float(payload.get("latitude"), "latitude", -90.0, 90.0)
        longitude = _safe_float(payload.get("longitude"), "longitude", -180.0, 180.0)
        timezone_name = str(payload.get("timezone", "")).strip() or "auto"

        data = {
            "location_name": location_name,
            "latitude": latitude,
            "longitude": longitude,
            "timezone": timezone_name,
            "updated_at": iso_now(),
        }

        Config.ensure_directories()
        Config.WEATHER_CONFIG_PATH.write_text(json.dumps(data, indent=2), encoding="utf-8")
        _apply_weather_config(data)

        with runtime.lock:
            runtime.weather_cache = {"updated_at": None, "data": None, "error": None}

        add_event("WEATHER_LOCATION_UPDATED", extra={"location": location_name, "latitude": latitude, "longitude": longitude})

        return {"ok": True, "config": get_weather_config(), "weather": fetch_weather(force=True)}

    except ValueError as exc:
        return {"ok": False, "error": str(exc)}
    except Exception as exc:
        return {"ok": False, "error": f"Could not save weather config: {exc}"}


def cache_weather(result: Dict[str, Any]) -> None:
    with runtime.lock:
        runtime.weather_cache["updated_at"] = utc_now()
        runtime.weather_cache["data"] = result
        runtime.weather_cache["error"] = result.get("error")


def fetch_weather(force: bool = False) -> Dict[str, Any]:
    now = utc_now()

    with runtime.lock:
        cached_at = runtime.weather_cache.get("updated_at")
        cached_data = runtime.weather_cache.get("data")

    if not force and cached_at and cached_data:
        if (now - cached_at).total_seconds() < Config.WEATHER_CACHE_SECONDS:
            return cached_data

    if not weather_configured():
        result = {
            "available": False,
            "location": Config.WEATHER_LOCATION_NAME,
            "rain_likely": False,
            "recommendation": "Weather fallback not configured",
            "error": "WEATHER_LAT and WEATHER_LON are missing.",
        }
        cache_weather(result)
        return result

    try:
        url = "https://api.open-meteo.com/v1/forecast"
        params = {
            "latitude": Config.WEATHER_LAT,
            "longitude": Config.WEATHER_LON,
            "hourly": "precipitation_probability,precipitation,rain",
            "forecast_days": 1,
            "timezone": Config.WEATHER_TIMEZONE,
        }

        response = requests.get(url, params=params, timeout=8)
        response.raise_for_status()
        data = response.json()

        hourly = data.get("hourly", {})
        times = hourly.get("time", [])
        probabilities = hourly.get("precipitation_probability", [])
        precipitation = hourly.get("precipitation", [])
        rain = hourly.get("rain", [])

        n = max(1, Config.WEATHER_LOOKAHEAD_HOURS)

        next_probs = [p for p in probabilities[:n] if isinstance(p, (int, float))]
        next_precip = [p for p in precipitation[:n] if isinstance(p, (int, float))]
        next_rain = [p for p in rain[:n] if isinstance(p, (int, float))]

        max_prob = max(next_probs) if next_probs else None
        total_precip = round(sum(next_precip), 2) if next_precip else 0.0
        total_rain = round(sum(next_rain), 2) if next_rain else 0.0

        rain_likely = bool(
            (max_prob is not None and max_prob >= Config.WEATHER_RAIN_PROB_THRESHOLD)
            or total_precip > Config.WEATHER_PRECIP_MM_THRESHOLD
            or total_rain > Config.WEATHER_PRECIP_MM_THRESHOLD
        )

        result = {
            "available": True,
            "location": Config.WEATHER_LOCATION_NAME,
            "latitude": Config.WEATHER_LAT,
            "longitude": Config.WEATHER_LON,
            "source": "Open-Meteo",
            "rain_likely": rain_likely,
            "max_precip_probability_next_hours": max_prob,
            "total_precip_next_hours_mm": total_precip,
            "total_rain_next_hours_mm": total_rain,
            "lookahead_hours": n,
            "probability_threshold_percent": Config.WEATHER_RAIN_PROB_THRESHOLD,
            "precip_threshold_mm": Config.WEATHER_PRECIP_MM_THRESHOLD,
            "recommendation": "Move to safe recommended" if rain_likely else "Stay drying",
            "sample_time": times[:n],
            "checked_at": iso_now(),
            "error": None,
        }

        cache_weather(result)
        return result

    except Exception as exc:
        result = {
            "available": False,
            "location": Config.WEATHER_LOCATION_NAME,
            "rain_likely": False,
            "recommendation": "Weather API unavailable",
            "checked_at": iso_now(),
            "error": str(exc),
        }
        cache_weather(result)
        return result


def parse_int(value: Any) -> Optional[int]:
    try:
        if value is None or value == "":
            return None
        return int(value)
    except (TypeError, ValueError):
        return None


def evaluate_rain_sensor(payload: Dict[str, Any]) -> Dict[str, Any]:
    rain_raw = parse_int(payload.get("rain_raw"))
    rain_status = str(payload.get("rain_status", "UNKNOWN")).upper().strip()

    reasons = []

    if rain_raw is None:
        reasons.append("RAIN_RAW_MISSING")
    elif rain_raw < 100 or rain_raw > 4095:
        reasons.append("RAIN_RAW_OUT_OF_RANGE")

    if rain_status in {"UNKNOWN", "", "NONE"}:
        reasons.append("RAIN_STATUS_UNKNOWN")

    now = utc_now()

    with runtime.lock:
        if rain_raw is not None:
            if runtime.last_rain_raw is None or runtime.last_rain_raw != rain_raw:
                runtime.last_rain_raw = rain_raw
                runtime.last_rain_change_time = now
            stuck_age = age_seconds(runtime.last_rain_change_time)
        else:
            stuck_age = None

    stuck_warning = bool(stuck_age is not None and stuck_age > 300)

    # A sensor frozen at the same ADC value for >5 minutes is unreliable — treat it as a
    # fault so the weather-API fallback kicks in, even if the raw value is in range.
    if stuck_warning:
        reasons.append("RAIN_RAW_STUCK")

    return {
        "healthy": len(reasons) == 0,
        "fault": len(reasons) > 0,
        "reasons": reasons,
        "rain_raw": rain_raw,
        "rain_status": rain_status,
        "stuck_warning": stuck_warning,
    }


def build_fallback_decision(payload: Optional[Dict[str, Any]] = None, telemetry_is_live: bool = True) -> Dict[str, Any]:
    if not Config.FALLBACK_ENABLED:
        return {
            "enabled": False,
            "active": False,
            "reason": "FALLBACK_DISABLED",
            "recommended_command": "NONE",
            "weather_rain_likely": False,
        }

    if not telemetry_is_live:
        weather = fetch_weather(force=False)
        return {
            "enabled": True,
            "active": False,
            "reason": "TELEMETRY_STALE_CANNOT_COMMAND",
            "recommended_command": "NONE",
            "note": "Nano is not communicating, so backend cannot send command until telemetry resumes.",
            "weather_rain_likely": bool(weather.get("rain_likely")),
            "weather": weather,
        }

    data = payload or {}
    state = str(data.get("state", "")).upper()
    target = str(data.get("target", "")).upper()

    rain_eval = evaluate_rain_sensor(data)
    rain_status = rain_eval["rain_status"]

    if rain_status == "CONFIRMED":
        should_move = state != "SAFE" and target != "SAFE"
        return {
            "enabled": True,
            "active": should_move,
            "reason": "LOCAL_RAIN_CONFIRMED",
            "recommended_command": "MOVE_TO_SAFE" if should_move else "NONE",
            "rain_sensor": rain_eval,
            "weather_rain_likely": None,
        }

    if rain_eval["healthy"]:
        # Sensor recovered — clear the fault window so the next fault triggers a fresh
        # weather fetch again.
        with runtime.lock:
            if runtime.last_sensor_fault_time is not None:
                runtime.last_sensor_fault_time = None
                add_event("RAIN_SENSOR_RECOVERED", state=state, target=target)
        return {
            "enabled": True,
            "active": False,
            "reason": "LOCAL_SENSOR_OK",
            "recommended_command": "NONE",
            "rain_sensor": rain_eval,
            "weather_rain_likely": None,
        }

    # ---------------------------------------------------------------
    # RAIN SENSOR IS FAULTY — use the weather API as the fallback.
    # Fault conditions that reach here:
    #   • rain_raw missing or out of range 100-4095
    #   • rain_status is UNKNOWN / empty
    #   • rain_raw stuck at the same ADC value for >5 minutes
    # ---------------------------------------------------------------

    now = utc_now()
    with runtime.lock:
        last_fault = runtime.last_sensor_fault_time
        is_new_fault = (last_fault is None)       # first tick of this fault window
        runtime.last_sensor_fault_time = now

    # Force a fresh Open-Meteo request the first time we enter a fault window so we
    # don't act on a potentially stale cache when the decision matters most.
    weather = fetch_weather(force=is_new_fault)

    weather_unavailable = not bool(weather.get("available"))
    # Conservative safety policy: if weather data itself is unavailable and the local
    # sensor cannot be trusted, default to MOVE_TO_SAFE (fail-safe).
    weather_bad = bool(weather.get("rain_likely")) or weather_unavailable

    should_move = (
        weather_bad
        and state != "SAFE"
        and target != "SAFE"
    )

    # Emit a single event when the fault is first detected (not on every telemetry tick).
    if is_new_fault:
        add_event(
            "RAIN_SENSOR_FAULT_DETECTED",
            state=state,
            target=target,
            extra={
                "fault_reasons": rain_eval["reasons"],
                "weather_available": bool(weather.get("available")),
                "weather_rain_likely": bool(weather.get("rain_likely")),
                "recommended_command": "MOVE_TO_SAFE" if should_move else "NONE",
            },
        )

    # Give each outcome a distinct reason code for easier log filtering.
    if weather_unavailable:
        reason = "RAIN_SENSOR_FAULT_WEATHER_UNAVAILABLE_DEFAULT_SAFE"
    elif weather_bad:
        reason = "RAIN_SENSOR_FAULT_WEATHER_RAIN_LIKELY"
    else:
        reason = "RAIN_SENSOR_FAULT_WEATHER_CLEAR"

    return {
        "enabled": True,
        "active": should_move,
        "reason": reason,
        "recommended_command": "MOVE_TO_SAFE" if should_move else "NONE",
        "rain_sensor": rain_eval,
        "weather_rain_likely": weather_bad,
        "weather": weather,
    }


def build_fallback_payload() -> Dict[str, Any]:
    status = get_data_status()

    with runtime.lock:
        latest = dict(runtime.latest_data)

    if status in {"STALE", "NO_DATA"}:
        return build_fallback_decision(latest, telemetry_is_live=False)

    return build_fallback_decision(latest, telemetry_is_live=True)


# ----------------------------
# TELEMETRY PROCESSING
# ----------------------------

def normalize_telemetry(payload: Dict[str, Any], remote_ip: Optional[str] = None, source: str = "mqtt") -> Dict[str, Any]:
    normalized = {
        "timestamp": iso_now(),
        "source": source,
        "state": str(payload.get("state", "UNKNOWN")),
        "target": str(payload.get("target", "UNKNOWN")),
        "event": str(payload.get("event", "NONE")),
        "line_pattern": str(payload.get("line_pattern", "---")),
        "rain_raw": payload.get("rain_raw", None),
        "rain_status": str(payload.get("rain_status", "UNKNOWN")),
        "left_lux": payload.get("left_lux", None),
        "right_lux": payload.get("right_lux", None),
        "light_mode": payload.get("light_mode", None),
        "line_lost_fault": payload.get("line_lost_fault", None),
        "movement_timeout_fault": payload.get("movement_timeout_fault", None),
        "light_tracking_timeout_fault": payload.get("light_tracking_timeout_fault", None),
        "light_left_ok": payload.get("light_left_ok", None),
        "light_right_ok": payload.get("light_right_ok", None),
        "laundry_active": payload.get("laundry_active", None),
        "ip": remote_ip,
    }

    for key, value in payload.items():
        if key not in normalized:
            normalized[key] = value

    return normalized


def process_telemetry(payload: Dict[str, Any], remote_ip: Optional[str] = None, source: str = "mqtt") -> Dict[str, Any]:
    row = normalize_telemetry(payload, remote_ip, source)
    fallback = build_fallback_decision(row, telemetry_is_live=True)

    command_to_send = "NONE"
    command_source = "NONE"

    if source == "mqtt":
        if "laundry_active" in row and isinstance(row.get("laundry_active"), bool):
            with runtime.lock:
                runtime.laundry_active = bool(row["laundry_active"])

        if fallback.get("recommended_command") != "NONE":
            command_to_send = str(fallback["recommended_command"])
            command_source = "FALLBACK_MQTT"
            publish_mqtt_command(command_to_send, source="weather_fallback", reason=fallback.get("reason"))

    else:
        # Legacy HTTP mode: return command in the response.
        with runtime.lock:
            command_to_send = runtime.pending_command
            runtime.pending_command = "NONE"

        command_source = "MANUAL_HTTP" if command_to_send != "NONE" else "NONE"

        if command_to_send == "NONE" and fallback.get("recommended_command") != "NONE":
            command_to_send = str(fallback["recommended_command"])
            command_source = "FALLBACK_HTTP"

    with runtime.lock:
        runtime.latest_data = row
        runtime.last_update_time = utc_now()

        add_event(
            row.get("event", "NONE"),
            state=str(row.get("state", "")),
            target=str(row.get("target", "")),
            extra={
                "source": source,
                "line_pattern": row.get("line_pattern"),
                "rain_status": row.get("rain_status"),
            },
        )

        active = runtime.laundry_active

    append_csv(
        row=row,
        command_sent=command_to_send,
        command_source=command_source,
        fallback=fallback,
        current_status=get_data_status(),
    )

    return {
        "ok": True,
        "mode": "mqtt" if source == "mqtt" else "http_legacy",
        "command": command_to_send,
        "command_source": command_source,
        "laundry_active": active,
        "fallback": {
            "enabled": fallback.get("enabled"),
            "active": fallback.get("active"),
            "reason": fallback.get("reason"),
            "weather_rain_likely": fallback.get("weather_rain_likely"),
        },
        "server_time": iso_now(),
    }


# ----------------------------
# CSV / LOGGING
# ----------------------------

def append_csv(row: Dict[str, Any], command_sent: str, command_source: str, fallback: Dict[str, Any], current_status: str) -> None:
    Config.ensure_directories()
    file_exists = Config.CSV_PATH.exists()

    with Config.CSV_PATH.open("a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDNAMES, extrasaction="ignore")

        if not file_exists:
            writer.writeheader()

        out = dict(row)
        out["command_sent"] = command_sent
        out["command_source"] = command_source
        out["fallback_active"] = fallback.get("active")
        out["fallback_reason"] = fallback.get("reason")
        out["weather_rain_likely"] = fallback.get("weather_rain_likely")
        out["data_status"] = current_status

        writer.writerow(out)


def get_recent_logs(limit: int = 50) -> List[Dict[str, Any]]:
    if not Config.CSV_PATH.exists():
        return []

    with Config.CSV_PATH.open("r", newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    return rows[-limit:][::-1]


def download_csv_text() -> str:
    if not Config.CSV_PATH.exists():
        return ""
    return Config.CSV_PATH.read_text(encoding="utf-8")


# ----------------------------
# DASHBOARD PAYLOADS
# ----------------------------

def build_status_payload() -> Dict[str, Any]:
    with runtime.lock:
        latest_copy = dict(runtime.latest_data)
        pending = runtime.pending_command
        active = runtime.laundry_active
        last_time = runtime.last_update_time
        events = list(runtime.event_log[:20])
        last_command = runtime.last_command_published

    status = get_data_status()
    fallback = build_fallback_payload()

    recommendation = "No telemetry yet"

    if status == "STALE":
        recommendation = "Telemetry stale - check Nano/MQTT connection"
    elif latest_copy.get("rain_status") == "CONFIRMED":
        recommendation = "Rain confirmed - robot should be moving safe or already safe"
    elif fallback.get("active"):
        recommendation = "Weather backup recommends moving to safe"
    elif latest_copy.get("state") == "SAFE":
        recommendation = "Robot is safe"
    elif latest_copy.get("state") == "DRYING":
        recommendation = "Robot is drying"
    else:
        recommendation = "Monitor robot"

    weather = fetch_weather(force=False)

    return {
        "latest": latest_copy,
        "data_status": status,
        "last_update_age_seconds": age_seconds(last_time),
        "pending_command": pending,
        "pending_command_label": command_label(pending),
        "last_command_published": last_command,
        "laundry_active": active,
        "recommendation": recommendation,
        "weather": weather,
        "weather_config": get_weather_config(),
        "fallback": fallback,
        "mqtt": build_mqtt_payload(),
        "events": events,
        "server_time": iso_now(),
        "stale_after_seconds": Config.STALE_AFTER_SECONDS,
    }


def build_health_payload() -> Dict[str, Any]:
    return {
        "server": "ok",
        "data_status": get_data_status(),
        "last_update_age_seconds": age_seconds(runtime.last_update_time),
        "weather_configured": weather_configured(),
        "weather_config": get_weather_config(),
        "fallback_enabled": Config.FALLBACK_ENABLED,
        "mqtt": build_mqtt_payload(),
        "server_time": iso_now(),
    }


def get_openapi_spec() -> Dict[str, Any]:
    try:
        return json.loads(Config.OPENAPI_PATH.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return {
            "openapi": "3.0.3",
            "info": {"title": "Smart Clothesline Robot API", "version": "1.0.0"},
            "paths": {},
        }
