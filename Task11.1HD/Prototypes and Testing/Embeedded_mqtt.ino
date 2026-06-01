/*
  Smart Clothesline Robot
  Lock-Based Hunt + Explicit Zone Codes + Rain + MQTT + Stable Zone Verification + Forward Commit 250ms

  Zone codes:
  DRYING = 111 -> 010
  SAFE   = 111 -> 000 -> 111

  L298N:
  ENA/ENB jumpers stay ON
  IN1 = 2
  IN2 = 3
  IN3 = 4
  IN4 = 7

  Line sensors:
  LEFT   = 8
  CENTER = 9
  RIGHT  = 10

  TCRT5000:
  Black tape = LOW

  Rain:
  If rain is confirmed while in DRYING, robot goes to SAFE.

  MQTT:
  Publishes telemetry to public MQTT broker.
  Subscribes for commands:
  MOVE_TO_SAFE, RETURN_TO_DRYING, STOP, SET_ACTIVE_TRUE, SET_ACTIVE_FALSE

  Serial:
  s = go to SAFE
  d = go to DRYING
  x = stop
*/

#include <Wire.h>
#include <BH1750.h>
#include <WiFiNINA.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// -------------------- PINS --------------------

const int IN1 = 2;
const int IN2 = 3;
const int IN3 = 4;
const int IN4 = 7;

const int LINE_LEFT   = 8;
const int LINE_CENTER = 9;
const int LINE_RIGHT  = 10;


const int BLACK_DETECTED = LOW;

// -------------------- RAIN SENSOR --------------------

const int RAIN_PIN = A0;

// Your calibration:
// dry ≈ 4000
// wet ≈ 2900
const int RAIN_THRESHOLD = 3500;
const unsigned long RAIN_CONFIRM_MS = 500;

int rainRaw = 0;
bool rainWetNow = false;
bool rainConfirmed = false;
unsigned long rainWetStartTime = 0;

// -------------------- MQTT / WIFI --------------------

// Replace with your WiFi details.
char WIFI_SSID[] = "S23 FE";
char WIFI_PASSWORD[] = "manit1234";

// Public MQTT broker.
// For demo/testing only. Public brokers are not private or guaranteed.
const char MQTT_BROKER[] = "broker.hivemq.com";
const int MQTT_PORT = 1883;

// Use unique topics so random public-broker users do not collide with your project.
const char MQTT_TELEMETRY_TOPIC[] = "smartclothesline/manitkhera26/demo01/telemetry";
const char MQTT_EVENT_TOPIC[]     = "smartclothesline/manitkhera26/demo01/event";
const char MQTT_COMMAND_TOPIC[]   = "smartclothesline/manitkhera26/demo01/command";
const char MQTT_STATUS_TOPIC[]    = "smartclothesline/manitkhera26/demo01/status";

// Normal telemetry publish interval.
// Telemetry is NOT published during movement-critical states.
const unsigned long TELEMETRY_INTERVAL_MS = 5000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 15000;
const unsigned long MQTT_RETRY_INTERVAL_MS = 8000;

// No full telemetry publishing during line tracking / movement.
// MQTT loop still runs so subscribed commands can arrive without HTTP polling.
const bool SEND_TELEMETRY_DURING_MOVEMENT = false;

// If MQTT disconnects while moving, do not reconnect until robot is idle.
// This avoids blocking movement with reconnect attempts.
const bool MQTT_RECONNECT_DURING_MOVEMENT = false;

WiFiClient mqttNetClient;
PubSubClient mqttClient(mqttNetClient);

unsigned long lastTelemetryTime = 0;
unsigned long lastWiFiRetryTime = 0;
unsigned long lastMqttRetryTime = 0;

bool wifiReady = false;
bool mqttReady = false;
bool backendReady = false; // kept for status compatibility; mirrors mqttReady

String lastBackendCommand = "NONE";
String lastMqttCommandSource = "NONE";

// Laundry mode:
// ON  = sun tracking and rain auto-safe active.
// OFF = stop sun tracking and ignore rain auto-safe; manual commands still work.
bool laundryActive = true;

// Confirmed motor direction config
const bool LEFT_MOTOR_REVERSED = true;
const bool RIGHT_MOTOR_REVERSED = false;

// -------------------- LIGHT SENSORS --------------------

BH1750 lightLeft;
BH1750 lightRight;

bool leftLightOK = false;
bool rightLightOK = false;

float latestLeftLux = -1;
float latestRightLux = -1;

// -------------------- MOTOR TUNING --------------------

const unsigned long FORWARD_PULSE_PERIOD_MS = 120;
const unsigned long LEFT_FORWARD_ON_MS  = 28;
const unsigned long RIGHT_FORWARD_ON_MS = 28;

const unsigned long TURN_PULSE_PERIOD_MS = 140;
const unsigned long TURN_ON_MS = 24;

const unsigned long TURN_180_MS = 2200;

// -------------------- HUNT TUNING --------------------

const int HUNT_ALLOWED_MISSES = 4;
const unsigned long HUNT_LOCK_TIMEOUT_MS = 1400;

// -------------------- MARKER VERIFICATION --------------------

const unsigned long MARKER_VERIFY_TIMEOUT_MS = 5000;

// Stable confirmation counts for zone classification.
// This reduces false SAFE/DRYING detection from one bad sensor reading.
const int DRYING_CENTER_CONFIRM_COUNT = 3;      // 111 -> stable 010
const int SAFE_GAP_CONFIRM_COUNT = 3;           // 111 -> stable 000
const int SAFE_SECOND_MARKER_CONFIRM_COUNT = 2; // confirmed 000 -> stable 111

// -------------------- FORWARD COMMIT TUNING --------------------

// After the robot finds 010, trust the current heading briefly and
// keep using the same pulsed forward() motor movement.
const unsigned long FORWARD_COMMIT_MS = 250;

// Require repeated side readings before correcting.
// This avoids twitchy over-correction.
const int SIDE_CONFIRM_COUNT = 2;

// -------------------- SUN TRACKING --------------------

const bool ENABLE_SUN_TRACKING = true;
const unsigned long LIGHT_SAMPLE_MS = 900;
const unsigned long SUN_ROTATE_STEP_MS = 220;
const float LIGHT_DEADBAND = 20.0;

// -------------------- PRINTING --------------------

const unsigned long PRINT_INTERVAL_MS = 250;

// -------------------- ROBOT STATES --------------------

enum RobotState {
  DRYING,
  SAFE,
  LINE_HUNT,
  MOVING,
  MARKER_LEAVE_FIRST,
  MARKER_VERIFY_SCAN,
  TURNING_AROUND,
  STOPPED
};

enum TargetZone {
  TARGET_NONE,
  TARGET_SAFE,
  TARGET_DRYING
};

enum DetectedZone {
  ZONE_NONE,
  ZONE_SAFE,
  ZONE_DRYING,
  ZONE_UNKNOWN
};

enum HuntState {
  HUNT_SEARCHING,
  HUNT_LOCK_LEFT,
  HUNT_LOCK_RIGHT
};

RobotState state = DRYING;
TargetZone target = TARGET_NONE;
TargetZone pendingTargetAfterTurn = TARGET_NONE;

HuntState huntState = HUNT_SEARCHING;

// -------------------- GLOBALS --------------------

unsigned long lastPrintTime = 0;
unsigned long turnStartTime = 0;
unsigned long markerVerifyStartTime = 0;
unsigned long lastLightSampleTime = 0;
unsigned long sunRotateUntil = 0;

unsigned long huntLockStartTime = 0;
int huntMissCount = 0;

// -1 = left, 1 = right
int huntDirection = 1;

// -1 = left, 0 = straight/unknown, 1 = right
int lastCorrectionDirection = 0;

// -1 = left, 0 = none, 1 = right
int sunDirection = 0;

bool markerArmed = false;
bool currentlyOnMarker = false;
bool safeGapSeen = false;

int markerZeroCount = 0;
int markerCenterCount = 0;
int markerSecondMarkerCount = 0;

unsigned long forwardCommitUntil = 0;
int sideLeftCount = 0;
int sideRightCount = 0;

String lastEvent = "SYSTEM_STARTED";

// -------------------- SETUP --------------------

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  pinMode(RAIN_PIN, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(LINE_LEFT, INPUT);
  pinMode(LINE_CENTER, INPUT);
  pinMode(LINE_RIGHT, INPUT);

  stopMotors();

  Wire.begin();

  leftLightOK = lightLeft.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);
  rightLightOK = lightRight.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire);

  Serial.println();
  Serial.println("Smart Clothesline Robot - Explicit Zone Code Version");
  Serial.println("DRYING = 111 -> 010");
  Serial.println("SAFE   = 111 -> 000 -> 111");
  Serial.println();
  Serial.println("Commands:");
  Serial.println("s = go to SAFE");
  Serial.println("d = go to DRYING");
  Serial.println("x = stop");
  Serial.println();

  Serial.print("Left BH1750: ");
  Serial.println(leftLightOK ? "OK" : "NOT FOUND");

  Serial.print("Right BH1750: ");
  Serial.println(rightLightOK ? "OK" : "NOT FOUND");

  Serial.println();
  Serial.println("MQTT:");
  Serial.print("Broker: ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  Serial.print("Command topic: ");
  Serial.println(MQTT_COMMAND_TOPIC);

  setupWiFi();
  setupMQTT();

  setEvent("SYSTEM_STARTED");
}

// -------------------- LOOP --------------------

void loop() {
  handleSerial();
  updateRainSensor();
  updateRobot();
  updateMQTTCommunication();
  printStatus();
}

// -------------------- SERIAL --------------------

void handleSerial() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  if (cmd == 's' || cmd == 'S') {
    Serial.println("Command: GO_SAFE");
    goToTarget(TARGET_SAFE);
  }

  else if (cmd == 'd' || cmd == 'D') {
    Serial.println("Command: GO_DRYING");
    goToTarget(TARGET_DRYING);
  }

  else if (cmd == 'x' || cmd == 'X') {
    Serial.println("Command: STOP");
    emergencyStop();
  }
}


// -------------------- RAIN SENSOR LOGIC --------------------

void updateRainSensor() {
  rainRaw = analogRead(RAIN_PIN);
  rainWetNow = rainRaw < RAIN_THRESHOLD;

  // If laundry is inactive, still read rain for telemetry,
  // but do not automatically move the robot.
  if (!laundryActive) {
    rainConfirmed = false;
    rainWetStartTime = 0;
    return;
  }

  // Only auto-trigger rain response while in drying mode.
  // Do not interrupt while already moving, safe, stopped, or turning.
  if (state != DRYING) {
    if (!rainWetNow) {
      rainConfirmed = false;
      rainWetStartTime = 0;
    }
    return;
  }

  if (rainWetNow) {
    if (rainWetStartTime == 0) {
      rainWetStartTime = millis();
      setEvent("RAIN_DETECTED_WAITING_CONFIRM");
    }

    if (!rainConfirmed && millis() - rainWetStartTime >= RAIN_CONFIRM_MS) {
      rainConfirmed = true;
      setEvent("RAIN_CONFIRMED_GOING_SAFE");
      goToTarget(TARGET_SAFE);
    }
  }

  else {
    if (rainConfirmed) {
      setEvent("RAIN_CLEARED");
    }

    rainConfirmed = false;
    rainWetStartTime = 0;
  }
}

String rainStatusName() {
  if (rainConfirmed) return "CONFIRMED";
  if (rainWetNow) return "WET";
  return "DRY";
}

// -------------------- WIFI / MQTT LOGIC --------------------

bool wifiCredentialsConfigured() {
  return String(WIFI_SSID) != "YOUR_WIFI_NAME" && String(WIFI_PASSWORD) != "YOUR_WIFI_PASSWORD";
}

void setupWiFi() {
  if (!wifiCredentialsConfigured()) {
    Serial.println("WiFi not configured. Edit WIFI_SSID and WIFI_PASSWORD.");
    wifiReady = false;
    return;
  }

  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    wifiReady = true;
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiReady = false;
    Serial.println("WiFi connection failed. Robot logic will continue without MQTT.");
  }
}

void maintainWiFi() {
  if (!wifiCredentialsConfigured()) {
    wifiReady = false;
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiReady = true;
    return;
  }

  wifiReady = false;
  mqttReady = false;
  backendReady = false;

  if (millis() - lastWiFiRetryTime < WIFI_RETRY_INTERVAL_MS) {
    return;
  }

  lastWiFiRetryTime = millis();

  Serial.println("WiFi disconnected. Retrying...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool isMovementCriticalState() {
  return state == LINE_HUNT ||
         state == MOVING ||
         state == MARKER_LEAVE_FIRST ||
         state == MARKER_VERIFY_SCAN ||
         state == TURNING_AROUND;
}

void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // PubSubClient default packet size can be small.
  // 512 is enough for our telemetry JSON.
  mqttClient.setBufferSize(512);
}

String mqttClientId() {
  String clientId = "nano-smartclothesline-";
  clientId += String(random(0xffff), HEX);
  clientId += "-";
  clientId += String(millis(), HEX);
  return clientId;
}

bool connectMQTT() {
  if (!wifiReady) return false;

  String clientId = mqttClientId();

  Serial.print("Connecting MQTT as ");
  Serial.println(clientId);

  if (mqttClient.connect(clientId.c_str())) {
    mqttReady = true;
    backendReady = true;

    mqttClient.subscribe(MQTT_COMMAND_TOPIC);

    Serial.println("MQTT connected.");
    Serial.print("Subscribed: ");
    Serial.println(MQTT_COMMAND_TOPIC);

    publishStatusMessage("NANO_ONLINE");
    setEvent("MQTT_CONNECTED");

    return true;
  }

  mqttReady = false;
  backendReady = false;

  Serial.print("MQTT connect failed. State: ");
  Serial.println(mqttClient.state());

  return false;
}

void maintainMQTT() {
  if (!wifiReady) return;

  if (mqttClient.connected()) {
    mqttReady = true;
    backendReady = true;

    // Lightweight. This is not HTTP polling.
    // Needed so subscribed commands can arrive.
    mqttClient.loop();
    return;
  }

  mqttReady = false;
  backendReady = false;

  // Do not reconnect while moving unless explicitly allowed.
  if (isMovementCriticalState() && !MQTT_RECONNECT_DURING_MOVEMENT) {
    return;
  }

  if (millis() - lastMqttRetryTime < MQTT_RETRY_INTERVAL_MS) {
    return;
  }

  lastMqttRetryTime = millis();
  connectMQTT();
}

void updateMQTTCommunication() {
  maintainWiFi();

  if (!wifiReady) return;

  maintainMQTT();

  if (!mqttReady) return;

  if (millis() - lastTelemetryTime < TELEMETRY_INTERVAL_MS) return;

  // Keep movement stable: no full telemetry publish while moving.
  // MQTT loop still runs above for commands such as STOP.
  if (!SEND_TELEMETRY_DURING_MOVEMENT && isMovementCriticalState()) {
    return;
  }

  lastTelemetryTime = millis();

  publishTelemetryToMQTT();
}

void publishTelemetryToMQTT() {
  StaticJsonDocument<512> doc;

  byte pattern = readLinePattern();

  doc["device"] = "nano";
  doc["state"] = stateName(state);
  doc["target"] = targetName(target);
  doc["event"] = lastEvent;
  doc["line_pattern"] = patternToString(pattern);
  doc["rain_raw"] = rainRaw;
  doc["rain_status"] = rainStatusName();
  doc["left_lux"] = latestLeftLux;
  doc["right_lux"] = latestRightLux;
  doc["light_left_ok"] = leftLightOK;
  doc["light_right_ok"] = rightLightOK;
  doc["laundry_active"] = laundryActive;
  doc["wifi_ok"] = wifiReady;
  doc["mqtt_ok"] = mqttReady;
  doc["last_command"] = lastBackendCommand;

  char payload[512];
  size_t n = serializeJson(doc, payload, sizeof(payload));

  bool ok = mqttClient.publish(MQTT_TELEMETRY_TOPIC, payload, n);

  if (!ok) {
    Serial.println("MQTT telemetry publish failed.");
    mqttReady = false;
    backendReady = false;
    return;
  }

  Serial.print("MQTT telemetry published: ");
  Serial.println(MQTT_TELEMETRY_TOPIC);
}

void publishStatusMessage(String statusName) {
  if (!mqttClient.connected()) return;

  StaticJsonDocument<256> doc;
  doc["device"] = "nano";
  doc["status"] = statusName;
  doc["state"] = stateName(state);
  doc["event"] = lastEvent;
  doc["laundry_active"] = laundryActive;

  char payload[256];
  size_t n = serializeJson(doc, payload, sizeof(payload));

  mqttClient.publish(MQTT_STATUS_TOPIC, payload, n);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  message.trim();

  Serial.print("MQTT message [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  String command = "";

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, message);

  if (!err) {
    const char* cmd = doc["command"] | "";
    command = String(cmd);

    const char* source = doc["source"] | "MQTT";
    lastMqttCommandSource = String(source);

    if (doc.containsKey("laundry_active")) {
      laundryActive = doc["laundry_active"] | laundryActive;
    }
  } else {
    // Also allow plain commands like STOP or MOVE_TO_SAFE
    command = message;
    lastMqttCommandSource = "MQTT_PLAIN";
  }

  command.trim();
  command.toUpperCase();

  handleMQTTCommand(command);
}

void handleMQTTCommand(String command) {
  if (command == "NONE" || command.length() == 0) {
    return;
  }

  lastBackendCommand = command;

  if (command == "STOP") {
    setEvent("MQTT_CMD_STOP");
    emergencyStop();
    publishStatusMessage("CMD_STOP_EXECUTED");
    return;
  }

  if (command == "MOVE_TO_SAFE") {
    if (isMovementCriticalState()) {
      setEvent("MQTT_CMD_MOVE_SAFE_IGNORED_MOVING");
      publishStatusMessage("CMD_MOVE_SAFE_IGNORED_MOVING");
      return;
    }

    setEvent("MQTT_CMD_MOVE_TO_SAFE");

    delay(200);

    goToTarget(TARGET_SAFE);
    return;
  }

  if (command == "RETURN_TO_DRYING") {
    if (isMovementCriticalState()) {
      setEvent("MQTT_CMD_DRYING_IGNORED_MOVING");
      publishStatusMessage("CMD_DRYING_IGNORED_MOVING");
      return;
    }

    setEvent("MQTT_CMD_RETURN_TO_DRYING");
    delay(200);

    goToTarget(TARGET_DRYING);
    return;
  }

  if (command == "SET_ACTIVE_TRUE") {
    laundryActive = true;
    setEvent("LAUNDRY_ACTIVE_ON");
    publishStatusMessage("LAUNDRY_ACTIVE_ON");
    return;
  }

  if (command == "SET_ACTIVE_FALSE") {
    laundryActive = false;
    stopMotors();
    setEvent("LAUNDRY_ACTIVE_OFF");
    publishStatusMessage("LAUNDRY_ACTIVE_OFF");
    return;
  }

  setEvent("MQTT_CMD_UNKNOWN");
  publishStatusMessage("CMD_UNKNOWN");
}


// -------------------- MAIN UPDATE --------------------

void updateRobot() {
  if (state == DRYING) {
    if (laundryActive) {
      updateSunTracking();
    } else {
      stopMotors();
      setEvent("LAUNDRY_INACTIVE_SUN_TRACKING_OFF");
    }
  }

  else if (state == LINE_HUNT) {
    updateLineHunt();
  }

  else if (state == MOVING) {
    updateMoving();
  }

  else if (state == MARKER_LEAVE_FIRST) {
    updateMarkerLeaveFirst();
  }

  else if (state == MARKER_VERIFY_SCAN) {
    updateMarkerVerifyScan();
  }

  else if (state == TURNING_AROUND) {
    updateTurnAround();
  }
}

// -------------------- TARGET HANDLING --------------------

void goToTarget(TargetZone requestedTarget) {
  stopMotors();

  if (requestedTarget == TARGET_SAFE && state == SAFE) {
    setEvent("ALREADY_IN_SAFE_ZONE");
    return;
  }

  if (requestedTarget == TARGET_DRYING && state == DRYING) {
    setEvent("ALREADY_IN_DRYING_ZONE");
    return;
  }

  if (state == SAFE && requestedTarget == TARGET_DRYING) {
    beginTurnAround(requestedTarget);
    return;
  }

  beginLineHunt(requestedTarget);
}

void beginLineHunt(TargetZone newTarget) {
  target = newTarget;
  state = LINE_HUNT;

  markerArmed = false;
  currentlyOnMarker = false;

  huntState = HUNT_SEARCHING;
  huntMissCount = 0;
  huntLockStartTime = millis();

  if (lastCorrectionDirection < 0) {
    huntDirection = -1;
  } else if (lastCorrectionDirection > 0) {
    huntDirection = 1;
  }

  lastCorrectionDirection = 0;

  if (target == TARGET_SAFE) {
    setEvent("HUNTING_LINE_TO_SAFE");
  } else {
    setEvent("HUNTING_LINE_TO_DRYING");
  }
}

void beginTurnAround(TargetZone nextTarget) {
  stopMotors();

  pendingTargetAfterTurn = nextTarget;
  turnStartTime = millis();
  state = TURNING_AROUND;

  setEvent("TURN_AROUND_STARTED");
}

void updateTurnAround() {
  if (millis() - turnStartTime >= TURN_180_MS) {
    stopMotors();
    setEvent("TURN_AROUND_COMPLETE");
    beginLineHunt(pendingTargetAfterTurn);
    return;
  }

  rotateRight();
}

// -------------------- SUN TRACKING --------------------

void updateSunTracking() {
  if (!ENABLE_SUN_TRACKING || !leftLightOK || !rightLightOK) {
    stopMotors();
    return;
  }

  unsigned long now = millis();

  if (now < sunRotateUntil) {
    if (sunDirection < 0) {
      rotateLeft();
    } else if (sunDirection > 0) {
      rotateRight();
    } else {
      stopMotors();
    }
    return;
  }

  stopMotors();

  if (now - lastLightSampleTime < LIGHT_SAMPLE_MS) {
    return;
  }

  lastLightSampleTime = now;

  float leftLux = lightLeft.readLightLevel();
  float rightLux = lightRight.readLightLevel();

  latestLeftLux = leftLux;
  latestRightLux = rightLux;

  if (leftLux < 0 || rightLux < 0) {
    setEvent("LIGHT_SENSOR_READ_ERROR");
    stopMotors();
    return;
  }

  float diff = leftLux - rightLux;

  if (diff > -LIGHT_DEADBAND && diff < LIGHT_DEADBAND) {
    sunDirection = 0;
    setEvent("SUN_BALANCED");
    stopMotors();
  }

  else if (diff > LIGHT_DEADBAND) {
    sunDirection = -1;
    sunRotateUntil = now + SUN_ROTATE_STEP_MS;
    setEvent("SUN_ROTATE_LEFT");
  }

  else {
    sunDirection = 1;
    sunRotateUntil = now + SUN_ROTATE_STEP_MS;
    setEvent("SUN_ROTATE_RIGHT");
  }
}

// -------------------- LOCK-BASED HUNT LOGIC --------------------

void updateLineHunt() {
  byte pattern = readLinePattern();

  if (pattern == 0b010) {
    state = MOVING;
    markerArmed = false;
    currentlyOnMarker = false;

    huntState = HUNT_SEARCHING;
    huntMissCount = 0;
    lastCorrectionDirection = 0;

    if (target == TARGET_SAFE) {
      setEvent("LINE_CENTERED_MOVING_TO_SAFE");
    } else {
      setEvent("LINE_CENTERED_MOVING_TO_DRYING");
    }

    return;
  }

  if (pattern == 0b001 || pattern == 0b011) {
    huntState = HUNT_LOCK_RIGHT;
    huntDirection = 1;
    huntMissCount = 0;
    huntLockStartTime = millis();

    setEvent("LOCK_RIGHT_ROTATE_RIGHT");
    rotateRight();
    return;
  }

  if (pattern == 0b100 || pattern == 0b110) {
    huntState = HUNT_LOCK_LEFT;
    huntDirection = -1;
    huntMissCount = 0;
    huntLockStartTime = millis();

    setEvent("LOCK_LEFT_ROTATE_LEFT");
    rotateLeft();
    return;
  }

  if (pattern == 0b111) {
    setEvent("HUNT_ON_MARKER_CREEP_FORWARD");
    forward();
    return;
  }

  if (pattern == 0b000) {
    handleHuntNoBlack();
    return;
  }

  if (huntDirection < 0) {
    setEvent("HUNT_WEIRD_KEEP_LEFT");
    rotateLeft();
  } else {
    setEvent("HUNT_WEIRD_KEEP_RIGHT");
    rotateRight();
  }
}

void handleHuntNoBlack() {
  unsigned long now = millis();

  if (huntState == HUNT_LOCK_RIGHT) {
    huntMissCount++;

    if (huntMissCount <= HUNT_ALLOWED_MISSES ||
        now - huntLockStartTime <= HUNT_LOCK_TIMEOUT_MS) {
      setEvent("LOCK_RIGHT_IGNORE_000_KEEP_RIGHT");
      rotateRight();
      return;
    }

    huntState = HUNT_SEARCHING;
    huntDirection = -1;
    huntMissCount = 0;

    setEvent("LOCK_RIGHT_FAILED_SEARCH_LEFT");
    rotateLeft();
    return;
  }

  if (huntState == HUNT_LOCK_LEFT) {
    huntMissCount++;

    if (huntMissCount <= HUNT_ALLOWED_MISSES ||
        now - huntLockStartTime <= HUNT_LOCK_TIMEOUT_MS) {
      setEvent("LOCK_LEFT_IGNORE_000_KEEP_LEFT");
      rotateLeft();
      return;
    }

    huntState = HUNT_SEARCHING;
    huntDirection = 1;
    huntMissCount = 0;

    setEvent("LOCK_LEFT_FAILED_SEARCH_RIGHT");
    rotateRight();
    return;
  }

  if (huntDirection < 0) {
    setEvent("SEARCHING_NO_BLACK_LEFT");
    rotateLeft();
  } else {
    setEvent("SEARCHING_NO_BLACK_RIGHT");
    rotateRight();
  }
}


// -------------------- FORWARD COMMIT MOVING LOGIC --------------------

void handleMovingPatternWithCommit(byte pattern) {
  // Marker detection stays immediate.
  if (pattern == 0b111) {
    clearForwardCommit();

    if (!currentlyOnMarker) {
      currentlyOnMarker = true;
      beginMarkerVerification();
      return;
    }

    forward();
    return;
  } else {
    currentlyOnMarker = false;
  }

  if (pattern == 0b010) {
    resetForwardCommitCounters();
    lastCorrectionDirection = 0;
    startForwardCommit();
    forward();
    return;
  }

  if (pattern == 0b100 || pattern == 0b110) {
    sideLeftCount++;
    sideRightCount = 0;
    lastCorrectionDirection = -1;

    if (inForwardCommit() || sideLeftCount < SIDE_CONFIRM_COUNT) {
      setEvent("SIDE_LEFT_DELAY_FORWARD");
      forward();
      return;
    }

    sideLeftCount = 0;
    setEvent("SIDE_LEFT_CONFIRMED_CORRECT");
    strongLeft();
    return;
  }

  if (pattern == 0b001 || pattern == 0b011) {
    sideRightCount++;
    sideLeftCount = 0;
    lastCorrectionDirection = 1;

    if (inForwardCommit() || sideRightCount < SIDE_CONFIRM_COUNT) {
      setEvent("SIDE_RIGHT_DELAY_FORWARD");
      forward();
      return;
    }

    sideRightCount = 0;
    setEvent("SIDE_RIGHT_CONFIRMED_CORRECT");
    strongRight();
    return;
  }

  if (pattern == 0b000) {
    resetForwardCommitCounters();

    if (inForwardCommit()) {
      setEvent("FORWARD_COMMIT_000_KEEP_FORWARD");
      forward();
      return;
    }

    clearForwardCommit();
    setEvent("LINE_LOST_REHUNTING");
    beginLineHunt(target);
    return;
  }

  // Fallback for any odd partial pattern
  forward();
}


// -------------------- MOVING --------------------

void updateMoving() {
  byte pattern = readLinePattern();

  if (!markerArmed) {
    if (pattern != 0b111 && pattern != 0b000) {
      markerArmed = true;
      setEvent("MARKER_DETECTION_ARMED");
      if (pattern == 0b010) startForwardCommit();
    }

    handleMovingPatternWithCommit(pattern);
    return;
  }

  handleMovingPatternWithCommit(pattern);
}

// -------------------- MARKER VERIFICATION HELPERS --------------------

void resetMarkerVerificationCounters() {
  safeGapSeen = false;
  markerZeroCount = 0;
  markerCenterCount = 0;
  markerSecondMarkerCount = 0;
}

// -------------------- FORWARD COMMIT HELPERS --------------------

void startForwardCommit() {
  forwardCommitUntil = millis() + FORWARD_COMMIT_MS;
}

bool inForwardCommit() {
  return millis() < forwardCommitUntil;
}

void resetForwardCommitCounters() {
  sideLeftCount = 0;
  sideRightCount = 0;
}

void clearForwardCommit() {
  forwardCommitUntil = 0;
  resetForwardCommitCounters();
}

// -------------------- EXPLICIT MARKER VERIFICATION --------------------

void beginMarkerVerification() {
  state = MARKER_LEAVE_FIRST;
  markerVerifyStartTime = millis();
  resetMarkerVerificationCounters();
  clearForwardCommit();

  setEvent("MARKER_VERIFY_FIRST_111_FOUND");
}

void updateMarkerLeaveFirst() {
  byte pattern = readLinePattern();

  if (millis() - markerVerifyStartTime > MARKER_VERIFY_TIMEOUT_MS) {
    setEvent("MARKER_LEAVE_TIMEOUT_UNKNOWN");
    zoneDecision(ZONE_UNKNOWN);
    return;
  }

  // Still on the first 111 marker.
  if (pattern == 0b111) {
    forward();
    return;
  }

  // Left first marker. Now scan for DRYING or SAFE code.
  state = MARKER_VERIFY_SCAN;
  markerVerifyStartTime = millis();
  resetMarkerVerificationCounters();

  setEvent("MARKER_LEFT_FIRST_111_SCANNING_CODE");
}

void updateMarkerVerifyScan() {
  byte pattern = readLinePattern();

  if (millis() - markerVerifyStartTime > MARKER_VERIFY_TIMEOUT_MS) {
    setEvent("MARKER_SCAN_TIMEOUT_UNKNOWN");
    zoneDecision(ZONE_UNKNOWN);
    return;
  }

  // SAFE middle gap candidate:
  // SAFE code is 111 -> stable 000 -> stable 111.
  if (pattern == 0b000) {
    markerZeroCount++;
    markerCenterCount = 0;
    markerSecondMarkerCount = 0;

    if (!safeGapSeen && markerZeroCount >= SAFE_GAP_CONFIRM_COUNT) {
      safeGapSeen = true;
      setEvent("ZONE_CODE_SAFE_GAP_CONFIRMED");
    } else {
      setEvent("ZONE_CODE_000_CHECKING_SAFE_GAP");
    }

    forward();
    return;
  }

  // DRYING candidate:
  // DRYING code is 111 -> stable 010.
  if (pattern == 0b010) {
    markerCenterCount++;
    markerZeroCount = 0;

    if (!safeGapSeen && markerCenterCount >= DRYING_CENTER_CONFIRM_COUNT) {
      setEvent("ZONE_CODE_111_010_DRYING_CONFIRMED");
      zoneDecision(ZONE_DRYING);
      return;
    }

    setEvent("ZONE_CODE_010_CHECKING_DRYING");
    forward();
    return;
  }

  // SAFE final marker candidate:
  // Only valid after stable 000 gap has been confirmed.
  if (pattern == 0b111) {
    markerCenterCount = 0;

    if (safeGapSeen) {
      markerSecondMarkerCount++;

      if (markerSecondMarkerCount >= SAFE_SECOND_MARKER_CONFIRM_COUNT) {
        setEvent("ZONE_CODE_111_000_111_SAFE_CONFIRMED");
        zoneDecision(ZONE_SAFE);
        return;
      }

      setEvent("ZONE_CODE_SECOND_111_CHECKING_SAFE");
      forward();
      return;
    }

    markerZeroCount = 0;
    markerSecondMarkerCount = 0;
    setEvent("MARKER_STILL_ON_111");
    forward();
    return;
  }

  // Partial line readings after first marker:
  markerZeroCount = 0;
  markerCenterCount = 0;
  markerSecondMarkerCount = 0;
  guidedForwardNoHunt(pattern);
}

void zoneDecision(DetectedZone detectedZone) {
  stopMotors();

  if (detectedZone == ZONE_DRYING && target == TARGET_DRYING) {
    state = DRYING;
    target = TARGET_NONE;
    setEvent("DRYING_ZONE_REACHED_CONFIRMED");
    return;
  }

  if (detectedZone == ZONE_SAFE && target == TARGET_SAFE) {
    state = SAFE;
    target = TARGET_NONE;
    setEvent("SAFE_ZONE_REACHED_CONFIRMED");
    return;
  }

  if (detectedZone == ZONE_DRYING && target == TARGET_SAFE) {
    setEvent("WRONG_ZONE_DRYING_FOUND_NEED_SAFE");
    beginTurnAround(TARGET_SAFE);
    return;
  }

  if (detectedZone == ZONE_SAFE && target == TARGET_DRYING) {
    setEvent("WRONG_ZONE_SAFE_FOUND_NEED_DRYING");
    beginTurnAround(TARGET_DRYING);
    return;
  }

  if (detectedZone == ZONE_UNKNOWN) {
    setEvent("ZONE_UNKNOWN_REHUNT_TARGET");
    beginLineHunt(target);
    return;
  }
}

// Used only during marker verification.
// It does NOT call beginLineHunt() on 000.
void guidedForwardNoHunt(byte pattern) {
  switch (pattern) {
    case 0b010:
      forward();
      break;

    case 0b100:
    case 0b110:
      strongLeft();
      break;

    case 0b001:
    case 0b011:
      strongRight();
      break;

    case 0b111:
      forward();
      break;

    case 0b000:
      forward();
      break;

    default:
      forward();
      break;
  }
}

// -------------------- LINE FOLLOWING --------------------

void followLinePattern(byte pattern) {
  switch (pattern) {
    case 0b010:
      lastCorrectionDirection = 0;
      forward();
      break;

    case 0b100:
    case 0b110:
      lastCorrectionDirection = -1;
      strongLeft();
      break;

    case 0b001:
    case 0b011:
      lastCorrectionDirection = 1;
      strongRight();
      break;

    case 0b000:
      beginLineHunt(target);
      break;

    case 0b111:
      forward();
      break;

    default:
      forward();
      break;
  }
}

// -------------------- SENSOR HELPERS --------------------

byte readLinePattern() {
  bool leftBlack = digitalRead(LINE_LEFT) == BLACK_DETECTED;
  bool centerBlack = digitalRead(LINE_CENTER) == BLACK_DETECTED;
  bool rightBlack = digitalRead(LINE_RIGHT) == BLACK_DETECTED;

  byte pattern = 0;

  if (leftBlack) pattern |= 0b100;
  if (centerBlack) pattern |= 0b010;
  if (rightBlack) pattern |= 0b001;

  return pattern;
}

String patternToString(byte pattern) {
  String s = "";
  s += (pattern & 0b100) ? "1" : "0";
  s += (pattern & 0b010) ? "1" : "0";
  s += (pattern & 0b001) ? "1" : "0";
  return s;
}

// -------------------- MOTOR PULSE HELPERS --------------------

bool forwardPulseOn(unsigned long onMs) {
  return (millis() % FORWARD_PULSE_PERIOD_MS) < onMs;
}

bool turnPulseOn() {
  return (millis() % TURN_PULSE_PERIOD_MS) < TURN_ON_MS;
}

// -------------------- MOTOR DIRECTION HELPERS --------------------

void setLeftMotorForward() {
  if (!LEFT_MOTOR_REVERSED) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  }
}

void setLeftMotorBackward() {
  if (!LEFT_MOTOR_REVERSED) {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
  } else {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
  }
}

void stopLeftMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

void setRightMotorForward() {
  if (!RIGHT_MOTOR_REVERSED) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  }
}

void setRightMotorBackward() {
  if (!RIGHT_MOTOR_REVERSED) {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
  } else {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
  }
}

void stopRightMotor() {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// -------------------- MOVEMENT HELPERS --------------------

void forward() {
  if (forwardPulseOn(LEFT_FORWARD_ON_MS)) {
    setLeftMotorForward();
  } else {
    stopLeftMotor();
  }

  if (forwardPulseOn(RIGHT_FORWARD_ON_MS)) {
    setRightMotorForward();
  } else {
    stopRightMotor();
  }
}

void rotateRight() {
  if (turnPulseOn()) {
    setLeftMotorForward();
    setRightMotorBackward();
  } else {
    stopMotors();
  }
}

void rotateLeft() {
  if (turnPulseOn()) {
    setLeftMotorBackward();
    setRightMotorForward();
  } else {
    stopMotors();
  }
}

void strongLeft() {
  if (turnPulseOn()) {
    setLeftMotorBackward();
    setRightMotorForward();
  } else {
    stopMotors();
  }
}

void strongRight() {
  if (turnPulseOn()) {
    setLeftMotorForward();
    setRightMotorBackward();
  } else {
    stopMotors();
  }
}

void stopMotors() {
  stopLeftMotor();
  stopRightMotor();
}

// -------------------- STOP --------------------

void emergencyStop() {
  stopMotors();
  state = STOPPED;
  target = TARGET_NONE;
  setEvent("EMERGENCY_STOPPED");
}

// -------------------- STATUS --------------------

void printStatus() {
  if (millis() - lastPrintTime < PRINT_INTERVAL_MS) return;
  lastPrintTime = millis();

  byte pattern = readLinePattern();

  Serial.print("State: ");
  Serial.print(stateName(state));

  Serial.print(" | Target: ");
  Serial.print(targetName(target));

  Serial.print(" | Pattern: ");
  Serial.print(patternToString(pattern));

  Serial.print(" | HuntState: ");
  Serial.print(huntStateName(huntState));

  Serial.print(" | HuntDir: ");
  Serial.print(huntDirection < 0 ? "LEFT" : "RIGHT");

  Serial.print(" | Misses: ");
  Serial.print(huntMissCount);

  Serial.print(" | RainRaw: ");
  Serial.print(rainRaw);

  Serial.print(" | Rain: ");
  Serial.print(rainStatusName());

  Serial.print(" | WiFi: ");
  Serial.print(wifiReady ? "OK" : "NO");

  Serial.print(" | MQTT: ");
  Serial.print(mqttReady ? "OK" : "NO");

  Serial.print(" | Cmd: ");
  Serial.print(lastBackendCommand);

  Serial.print(" | Active: ");
  Serial.print(laundryActive ? "YES" : "NO");

  Serial.print(" | GapSeen: ");
  Serial.print(safeGapSeen ? "YES" : "NO");

  Serial.print(" | Z0: ");
  Serial.print(markerZeroCount);

  Serial.print(" | ZC: ");
  Serial.print(markerCenterCount);

  Serial.print(" | Z111: ");
  Serial.print(markerSecondMarkerCount);

  Serial.print(" | Commit: ");
  Serial.print(inForwardCommit() ? "YES" : "NO");

  Serial.print(" | LSide: ");
  Serial.print(sideLeftCount);

  Serial.print(" | RSide: ");
  Serial.print(sideRightCount);

  Serial.print(" | Event: ");
  Serial.println(lastEvent);
}

void setEvent(String eventName) {
  if (lastEvent == eventName) return;

  lastEvent = eventName;
  Serial.print("EVENT: ");
  Serial.println(eventName);
}

String stateName(RobotState s) {
  switch (s) {
    case DRYING: return "DRYING";
    case SAFE: return "SAFE";
    case LINE_HUNT: return "LINE_HUNT";
    case MOVING: return "MOVING";
    case MARKER_LEAVE_FIRST: return "MARKER_LEAVE_FIRST";
    case MARKER_VERIFY_SCAN: return "MARKER_VERIFY_SCAN";
    case TURNING_AROUND: return "TURNING_AROUND";
    case STOPPED: return "STOPPED";
    default: return "UNKNOWN";
  }
}

String targetName(TargetZone t) {
  switch (t) {
    case TARGET_NONE: return "NONE";
    case TARGET_SAFE: return "SAFE";
    case TARGET_DRYING: return "DRYING";
    default: return "UNKNOWN";
  }
}

String huntStateName(HuntState h) {
  switch (h) {
    case HUNT_SEARCHING: return "SEARCHING";
    case HUNT_LOCK_LEFT: return "LOCK_LEFT";
    case HUNT_LOCK_RIGHT: return "LOCK_RIGHT";
    default: return "UNKNOWN";
  }
}
