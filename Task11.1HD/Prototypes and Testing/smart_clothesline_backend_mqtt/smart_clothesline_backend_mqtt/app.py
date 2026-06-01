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
    process_telemetry,
    publish_manual_command,
    start_mqtt_bridge,
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

    # =============================================================================
# SMART CLOTHESLINE BACKEND / FRONTEND FLOW REFERENCE
# =============================================================================
#
# HIGH-LEVEL SYSTEM MODEL
# -----------------------
# The browser dashboard does NOT talk to the Nano directly.
# The browser talks to Flask using HTTP endpoints.
# Flask talks to the Nano using MQTT.
#
# Main direction 1: manual command
#
#   Browser dashboard
#       -> Flask endpoint /api/command
#       -> Backend publishes MQTT command
#       -> Nano receives MQTT command
#       -> Nano executes robot behaviour
#
# Main direction 2: telemetry/status display
#
#   Nano publishes telemetry/status to MQTT
#       -> Backend receives MQTT message
#       -> Backend stores latest robot data in memory
#       -> Browser calls /api/status
#       -> Dashboard updates displayed values
#
#
# =============================================================================
# MAIN FLOW 1: DASHBOARD COMMAND FLOW
# =============================================================================
#
# Starts from:
#   Frontend / browser
#
# Trigger:
#   User clicks a dashboard button.
#
# Frontend function:
#   sendCommand(command)
#
# Flask endpoint:
#   POST /api/command
#
# Backend function:
#   publish_manual_command(command)
#       -> validates command
#       -> calls publish_mqtt_command(...)
#
# MQTT topic published to:
#   smartclothesline/manitkhera26/demo01/command
#
# Nano behaviour:
#   Nano is subscribed to the command topic.
#   When a command message arrives, the Nano parses it and executes the command.
#
# Example path:
#
#   User clicks "Move to Safe"
#       -> dashboard.js calls sendCommand("MOVE_TO_SAFE")
#       -> browser sends POST /api/command
#       -> Flask receives {"command": "MOVE_TO_SAFE"}
#       -> backend publishes MQTT command payload
#       -> Nano receives command
#       -> Nano starts moving toward SAFE zone
#
# Supported commands:
#
#   MOVE_TO_SAFE
#   RETURN_TO_DRYING
#   STOP
#   SET_ACTIVE_TRUE
#   SET_ACTIVE_FALSE
#
#
# =============================================================================
# MAIN FLOW 2: NANO TELEMETRY FLOW
# =============================================================================
#
# Starts from:
#   Nano 33 IoT
#
# Trigger:
#   Nano periodically publishes sensor/state data.
#
# MQTT topic:
#   smartclothesline/manitkhera26/demo01/telemetry
#
# Backend MQTT callback:
#   _on_mqtt_message(...)
#
# Backend processing function:
#   process_telemetry(data)
#
# What telemetry contains:
#
#   state
#   target
#   event
#   line_pattern
#   rain_raw
#   rain_status
#   left_lux
#   right_lux
#   light_left_ok
#   light_right_ok
#   laundry_active
#   wifi_ok
#   mqtt_ok
#   last_command
#
# Backend actions after receiving telemetry:
#
#   1. Parse incoming MQTT JSON.
#   2. Normalise the robot data.
#   3. Store latest robot state in backend memory.
#   4. Add important events to the event list.
#   5. Append telemetry row to CSV log.
#   6. Make latest data available to /api/status.
#
# Important:
#   The frontend does NOT receive MQTT directly.
#   The frontend only sees telemetry later by calling /api/status.
#
#
# =============================================================================
# MAIN FLOW 3: DASHBOARD STATUS / REFRESH FLOW
# =============================================================================
#
# Starts from:
#   Frontend / browser
#
# Trigger:
#   dashboard.js calls refresh() when the page loads.
#   dashboard.js also calls refresh() repeatedly every second.
#
# Frontend function:
#   refresh()
#
# Flask endpoint:
#   GET /api/status
#
# Backend action:
#   Build and return the latest dashboard status payload.
#
# What /api/status returns:
#
#   latest robot telemetry
#   robot state
#   robot target
#   latest event
#   rain sensor status
#   light sensor readings
#   line sensor pattern
#   laundry active status
#   MQTT connection status
#   weather information
#   fallback/recommendation information
#   recent events
#   server time
#
# Important:
#   /api/status does NOT ask the Nano for new data.
#   It only returns the latest data already stored in the backend.
#
# Correct mental model:
#
#   Nano -> MQTT telemetry -> backend memory
#   Browser -> GET /api/status -> reads backend memory
#
#
# =============================================================================
# MAIN FLOW 4: LOG REFRESH FLOW
# =============================================================================
#
# Starts from:
#   Frontend / browser
#
# Trigger:
#   refresh() calls refreshLogs().
#
# Frontend function:
#   refreshLogs()
#
# Flask endpoint:
#   GET /api/logs?limit=20
#
# Backend action:
#   Reads recent rows from telemetry_log.csv.
#
# Frontend result:
#   Dashboard updates the "Recent Logs" table.
#
# Important:
#   This flow is display-only.
#   It does not send commands to the Nano.
#
#
# =============================================================================
# MAIN FLOW 5: LEGACY HTTP TELEMETRY FLOW // pointless to dicuss
# =============================================================================
#
# Starts from:
#   A test client, curl command, or older HTTP-based Nano version.
#
# Flask endpoint:
#   POST /api/telemetry
#
# Backend function:
#   process_telemetry(data)
#
# Purpose:
#   Allows telemetry to be posted directly to Flask using HTTP.
#
# Important:
#   This is NOT the main final communication path.
#   The final project mainly uses MQTT for Nano telemetry.
#
# Final telemetry path:
#
#   Nano -> MQTT telemetry topic -> backend MQTT callback -> process_telemetry()
#
# Legacy/testing path:
#
#   Test client or old Nano code -> POST /api/telemetry -> process_telemetry()
#
#
# =============================================================================
# MAIN FLOW 6: MQTT STATUS FLOW
# =============================================================================
#
# Starts from:
#   Nano / MQTT publisher
#
# MQTT topic:
#   smartclothesline/manitkhera26/demo01/status
#
# Backend behaviour:
#   Backend subscribes to the status topic.
#   Status messages are used to update backend events/status information.
#
# Frontend visibility:
#   The browser sees these updates later through GET /api/status.
#
#
# =============================================================================
# MAIN FLOW 7: WEATHER REFRESH FLOW
# =============================================================================
#
# Starts from:
#   Frontend / browser
#
# Trigger:
#   User clicks weather refresh button.
#
# Frontend function:
#   refreshWeather()
#
# Flask endpoint:
#   POST /api/weather/refresh
#
# Backend action:
#   Refreshes cached weather information.
#
# Frontend result:
#   Dashboard then calls refresh() again and displays updated weather data.
#
# Important:
#   This does not directly command the Nano.
#
#
# =============================================================================
# MAIN FLOW 8: FALLBACK CHECK FLOW
# =============================================================================
#
# Starts from:
#   Frontend / browser
#
# Trigger:
#   User clicks fallback check button.
#
# Frontend function:
#   checkFallback()
#
# Flask endpoint:
#   GET /api/fallback
#
# Backend action:
#   Calculates fallback recommendation based on backend logic.
#
# Frontend result:
#   Dashboard displays fallback reason and recommended command.
#
# Important:
#   Calling /api/fallback is mainly diagnostic.
#   It shows what the backend recommends.
#   It does not necessarily mean a command is immediately sent to the Nano.
#
#
# =============================================================================
# FRONTEND FUNCTION SUMMARY
# =============================================================================
#
# dashboard.js functions:
#
#   sendCommand(command)
#       -> POST /api/command
#       -> Used by buttons such as MOVE_TO_SAFE, RETURN_TO_DRYING, STOP,
#          SET_ACTIVE_TRUE, and SET_ACTIVE_FALSE.
#
#   refresh()
#       -> GET /api/status
#       -> Updates main dashboard values.
#       -> Also calls refreshLogs().
#
#   refreshLogs()
#       -> GET /api/logs?limit=20
#       -> Updates recent telemetry log table.
#
#   refreshWeather()
#       -> POST /api/weather/refresh
#       -> Refreshes cached weather data.
#       -> Then calls refresh().
#
#   checkFallback()
#       -> GET /api/fallback
#       -> Displays backend fallback recommendation.
#
#   text(id, value)
#       -> Helper function.
#       -> Updates text content of a dashboard element.
#
#   yesNo(value)
#       -> Helper function.
#       -> Converts booleans into display-friendly YES/NO values.
#
#
# =============================================================================
# BACKEND ENDPOINT SUMMARY
# =============================================================================
#
#   GET /
#       -> Serves the main dashboard HTML page.
#
#   POST /api/command
#       -> Receives dashboard commands over HTTP.
#       -> Publishes command to MQTT command topic.
#       -> Main bridge from frontend to Nano.
#
#   GET /api/status
#       -> Main dashboard polling endpoint.
#       -> Returns latest stored robot data, MQTT status, weather, fallback,
#          recent events, and server time.
#       -> Does not contact Nano directly.
#
#   GET /api/logs?limit=20
#       -> Returns recent telemetry CSV log entries.
#       -> Used by dashboard log table.
#
#   POST /api/telemetry
#       -> Legacy/testing endpoint for HTTP telemetry.
#       -> Not the main final Nano communication path.
#
#   GET /api/mqtt
#       -> Diagnostic endpoint for MQTT bridge status.
#
#   GET /api/health
#       -> Backend health check endpoint.
#
#   GET /api/weather
#       -> Returns cached weather data.
#
#   POST /api/weather/refresh
#       -> Forces weather data refresh.
#
#   GET /api/fallback
#       -> Returns backend fallback recommendation.
#
#   GET /api/events
#       -> Returns recent backend event log.
#
#   GET /logs.csv
#       -> Downloads telemetry CSV log file.
#
#   GET /docs
#       -> Opens Swagger/API documentation page.
#
#   GET /openapi.json
#       -> Returns OpenAPI schema used by Swagger.
#
#
# =============================================================================
# MQTT TOPIC SUMMARY
# =============================================================================
#
#   smartclothesline/manitkhera26/demo01/telemetry
#       -> Nano publishes telemetry here.
#       -> Backend subscribes to this.
#
#   smartclothesline/manitkhera26/demo01/status
#       -> Nano/backend status messages.
#       -> Backend subscribes to this.
#
#   smartclothesline/manitkhera26/demo01/command
#       -> Backend publishes commands here.
#       -> Nano subscribes to this.
#
#   smartclothesline/manitkhera26/demo01/event
#       -> Defined event topic.
#       -> In this version, important event data is mainly carried through
#          telemetry/status and displayed through /api/status.
#
#
# =============================================================================
# VIVA-SAFE EXPLANATION
# =============================================================================
#
# The dashboard and backend communicate using HTTP endpoints. The browser polls
# /api/status every second to read the latest robot state stored by the backend.
# The Nano does not send data directly to the browser. Instead, the Nano publishes
# telemetry to MQTT, the backend receives it, stores it, logs it, and exposes it
# to the dashboard through /api/status.
#
# Manual commands flow in the opposite direction. When a dashboard button is
# pressed, JavaScript sends POST /api/command to Flask. Flask validates the
# command and publishes it to the MQTT command topic. The Nano subscribes to that
# topic and executes supported commands such as MOVE_TO_SAFE, RETURN_TO_DRYING,
# STOP, SET_ACTIVE_TRUE, and SET_ACTIVE_FALSE.
#
# =============================================================================
