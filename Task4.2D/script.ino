#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>

char ssid[] = "";
char pass[] = "";

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

const char broker[] = "broker.emqx.io";
const int mqttPort = 1883;

const char commandTopic[] = "sit210/s224520987/lindaLights/command";
const char statusTopic[]  = "sit210/s224520987/lindaLights/status";

const int LED_LIVING = 3;
const int LED_BATH = 5;
const int LED_CLOSET = 8;

bool livingroom = false;
bool bathroom = false;
bool closet = false;

unsigned long livingOffAt = 0;
unsigned long bathOffAt = 0;
unsigned long closetOffAt = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(LED_LIVING, OUTPUT);
  pinMode(LED_BATH, OUTPUT);
  pinMode(LED_CLOSET, OUTPUT);

  digitalWrite(LED_LIVING, LOW);
  digitalWrite(LED_BATH, LOW);
  digitalWrite(LED_CLOSET, LOW);

  connectToWiFi();
  connectToMQTT();

  Serial.println("MQTT remote light control system ready");
}

void loop() {
  checkConnections();

  mqttClient.poll();

  checkTimers();
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(3000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("Arduino local IP: ");
  Serial.println(WiFi.localIP());
}

void connectToMQTT() {
  mqttClient.setId("arduinoNano33IoT_s224520987");
  mqttClient.onMessage(onMqttMessage);

  Serial.print("Connecting to MQTT broker");

  while (!mqttClient.connect(broker, mqttPort)) {
    Serial.print(".");
    delay(3000);
  }

  Serial.println();
  Serial.println("Connected to MQTT broker");

  mqttClient.subscribe(commandTopic);

  Serial.print("Subscribed to command topic: ");
  Serial.println(commandTopic);

  publishStatus();
}

void checkConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    connectToWiFi();
  }

  if (!mqttClient.connected()) {
    Serial.println("MQTT disconnected. Reconnecting...");
    connectToMQTT();
  }
}

void onMqttMessage(int messageSize) {
  String payload = "";

  while (mqttClient.available()) {
    payload += (char)mqttClient.read();
  }

  Serial.print("MQTT command received: ");
  Serial.println(payload);

  handleMqttCommand(payload);
}

String getJsonValue(String json, String key) {
  String pattern = "\"" + key + "\":";
  int keyIndex = json.indexOf(pattern);

  if (keyIndex == -1) {
    return "";
  }

  int valueStart = keyIndex + pattern.length();

  while (json.charAt(valueStart) == ' ') {
    valueStart++;
  }

  if (json.charAt(valueStart) == '"') {
    valueStart++;
    int valueEnd = json.indexOf("\"", valueStart);
    return json.substring(valueStart, valueEnd);
  } else {
    int valueEnd = json.indexOf(",", valueStart);

    if (valueEnd == -1) {
      valueEnd = json.indexOf("}", valueStart);
    }

    return json.substring(valueStart, valueEnd);
  }
}

void handleMqttCommand(String payload) {
  String room = getJsonValue(payload, "room");
  String action = getJsonValue(payload, "action");
  int seconds = getJsonValue(payload, "seconds").toInt();

  room.trim();
  action.trim();

  Serial.print("Room: ");
  Serial.println(room);

  Serial.print("Action: ");
  Serial.println(action);

  Serial.print("Seconds: ");
  Serial.println(seconds);

  if (room != "livingroom" && room != "bathroom" && room != "closet") {
    Serial.println("Invalid room received");
    return;
  }

  if (action == "toggle") {
    toggleRoom(room);
  }
  else if (action == "timer") {
    if (seconds > 0) {
      startTimer(room, seconds);
    } else {
      Serial.println("Invalid timer value");
    }
  }
  else {
    Serial.println("Invalid action received");
  }

  publishStatus();
}

void checkTimers() {
  unsigned long now = millis();

  if (livingOffAt != 0 && now >= livingOffAt) {
    livingroom = false;
    digitalWrite(LED_LIVING, LOW);
    livingOffAt = 0;

    Serial.println("Living room timer finished");
    publishStatus();
  }

  if (bathOffAt != 0 && now >= bathOffAt) {
    bathroom = false;
    digitalWrite(LED_BATH, LOW);
    bathOffAt = 0;

    Serial.println("Bathroom timer finished");
    publishStatus();
  }

  if (closetOffAt != 0 && now >= closetOffAt) {
    closet = false;
    digitalWrite(LED_CLOSET, LOW);
    closetOffAt = 0;

    Serial.println("Closet timer finished");
    publishStatus();
  }
}

void toggleRoom(String room) {
  if (room == "livingroom") {
    livingroom = !livingroom;
    digitalWrite(LED_LIVING, livingroom ? HIGH : LOW);
    if (!livingroom) livingOffAt = 0;
  }
  else if (room == "bathroom") {
    bathroom = !bathroom;
    digitalWrite(LED_BATH, bathroom ? HIGH : LOW);
    if (!bathroom) bathOffAt = 0;
  }
  else if (room == "closet") {
    closet = !closet;
    digitalWrite(LED_CLOSET, closet ? HIGH : LOW);
    if (!closet) closetOffAt = 0;
  }

  Serial.print(room);
  Serial.println(" toggled");
}

void startTimer(String room, int seconds) {
  unsigned long offTime = millis() + ((unsigned long)seconds * 1000UL);

  if (room == "livingroom") {
    livingroom = true;
    digitalWrite(LED_LIVING, HIGH);
    livingOffAt = offTime;
  }
  else if (room == "bathroom") {
    bathroom = true;
    digitalWrite(LED_BATH, HIGH);
    bathOffAt = offTime;
  }
  else if (room == "closet") {
    closet = true;
    digitalWrite(LED_CLOSET, HIGH);
    closetOffAt = offTime;
  }

  Serial.print("Timer started for ");
  Serial.print(room);
  Serial.print(" for ");
  Serial.print(seconds);
  Serial.println(" seconds");
}

void publishStatus() {
  String json = "{";
  json += "\"livingroom\":\"" + String(livingroom ? "ON" : "OFF") + "\",";
  json += "\"bathroom\":\"" + String(bathroom ? "ON" : "OFF") + "\",";
  json += "\"closet\":\"" + String(closet ? "ON" : "OFF") + "\"";
  json += "}";

  mqttClient.beginMessage(statusTopic);
  mqttClient.print(json);
  mqttClient.endMessage();

  Serial.print("Published status: ");
  Serial.println(json);
}