#!/usr/bin/env python3
"""
Smart Clothesline Robot Backend - MQTT version

app.py      = Flask routes
services.py = state, MQTT bridge, commands, CSV logging, Melbourne weather backup
"""

from flask import Flask, Response, jsonify, render_template, request

from services import (
    Config,
    build_fallback_payload,
    build_health_payload,
    build_mqtt_payload,
    build_status_payload,
    download_csv_text,
    fetch_weather,
    get_events,
    get_openapi_spec,
    get_recent_logs,
    get_weather_config,
    process_telemetry,
    publish_manual_command,
    start_mqtt_bridge,
    update_weather_config,
    weather_configured,
)


app = Flask(
    __name__,
    template_folder="templates",
    static_folder="wwwroot",
    static_url_path="/static",
)


@app.get("/")
def dashboard():
    return render_template("dashboard.html")


@app.get("/docs")
def docs():
    return render_template("swagger.html")


@app.get("/openapi.json")
def openapi_json():
    return jsonify(get_openapi_spec())


@app.post("/api/telemetry")
def telemetry():
    """
    Legacy HTTP telemetry endpoint.
    MQTT is now the main Nano communication path, but this remains useful for curl tests.
    """
    payload = request.get_json(silent=True)
    if not isinstance(payload, dict):
        return jsonify({"ok": False, "error": "Expected JSON object"}), 400

    return jsonify(process_telemetry(payload, remote_ip=request.remote_addr, source="http"))


@app.get("/api/status")
def status():
    return jsonify(build_status_payload())


@app.get("/api/health")
def health():
    return jsonify(build_health_payload())


@app.get("/api/mqtt")
def mqtt_status():
    return jsonify(build_mqtt_payload())


@app.post("/api/command")
def command():
    payload = request.get_json(silent=True) or {}
    result = publish_manual_command(str(payload.get("command", "")))
    if not result.get("ok"):
        return jsonify(result), 400
    return jsonify(result)


@app.get("/api/weather")
def weather():
    return jsonify(fetch_weather(force=False))


@app.post("/api/weather/refresh")
def weather_refresh():
    return jsonify({"ok": True, "weather": fetch_weather(force=True)})


@app.get("/api/weather/config")
def weather_config_get():
    return jsonify(get_weather_config())


@app.post("/api/weather/config")
def weather_config_update():
    payload = request.get_json(silent=True) or {}
    result = update_weather_config(payload)
    if not result.get("ok"):
        return jsonify(result), 400
    return jsonify(result)


@app.get("/api/fallback")
def fallback():
    return jsonify(build_fallback_payload())


@app.get("/api/logs")
def logs():
    limit = int(request.args.get("limit", "50"))
    limit = max(1, min(limit, 500))
    return jsonify(get_recent_logs(limit=limit))


@app.get("/api/events")
def events():
    return jsonify(get_events())


@app.get("/logs.csv")
def logs_csv():
    return Response(
        download_csv_text(),
        mimetype="text/csv",
        headers={"Content-Disposition": "attachment; filename=telemetry_log.csv"},
    )


if __name__ == "__main__":
    Config.ensure_directories()
    start_mqtt_bridge()

    print(f"Starting Smart Clothesline backend on http://{Config.HOST}:{Config.PORT}")
    print(f"Dashboard: http://<raspberry-pi-ip>:{Config.PORT}/")
    print(f"Swagger:   http://<raspberry-pi-ip>:{Config.PORT}/docs")
    print(f"CSV log:   {Config.CSV_PATH.resolve()}")

    print()
    print("MQTT bridge:")
    print(f"Broker:    {Config.MQTT_BROKER}:{Config.MQTT_PORT}")
    print(f"Telemetry: {Config.MQTT_TELEMETRY_TOPIC}")
    print(f"Status:    {Config.MQTT_STATUS_TOPIC}")
    print(f"Command:   {Config.MQTT_COMMAND_TOPIC}")

    if weather_configured():
        print(f"Weather fallback: {Config.WEATHER_LOCATION_NAME} ({Config.WEATHER_LAT}, {Config.WEATHER_LON})")
    else:
        print("Weather fallback not configured.")

    app.run(host=Config.HOST, port=Config.PORT, debug=Config.DEBUG)
