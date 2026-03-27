// WiFi + MQTT libraries
#include <WiFiNINA.h>
#include <PubSubClient.h>

// my wifi hotspot
char ssid[] = "";
char pass[] = "";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// MQTT broker details
const char broker[] = "broker.emqx.io";
const int port = 1883;

const char topicWave[] = "ES/Wave";
const char topicPat[]  = "ES/Pat";
const char myName[]    = "Manit";

// pin defs
const int trigPin = 6;   // D6
const int echoPin = 7;   // D7
const int led1Pin = 2;   // D2
const int led2Pin = 3;   // D3

// timers and intervals (in ms)
const unsigned long wifiRetryInterval = 3000;
const unsigned long mqttRetryInterval = 2000;
const unsigned long sampleInterval    = 60;
const unsigned long printInterval     = 500;
const unsigned long cooldownMs        = 1200; // wait a bit before next gesture

// gesture settings
const int gestureThresholdCm = 12;      // under this distance means hand is near
const unsigned long waveMinMs   = 350;  // hold hand over sensor to count as wave
const unsigned long patMinMs    = 40;   // min tap time
const unsigned long patMaxMs    = 220;  // max tap time
const unsigned long patWindowMs = 800;  // need 2 taps within this time limit

// state tracking vars
unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastSampleAt    = 0;
unsigned long lastPrintAt     = 0;
unsigned long lastTriggerAt   = 0;

bool wifiConnectedOnce = false;
bool mqttConnectedOnce = false;

bool objectNear = false;
unsigned long nearStartedAt = 0;

int quickHitCount = 0;
unsigned long firstHitAt = 0;

// function prototypes
void startWiFi();
void maintainWiFi();
void maintainMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
long readDistanceCm();
void detectGesture();
void publishWave();
void publishPat();
void ledsOn();
void ledsOff();

void setup() {
  Serial.begin(9600);
  unsigned long start = millis();

  // wait for serial monitor, but don't hang forever
  while (!Serial && millis() - start < 4000) {}

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(led1Pin, OUTPUT);
  pinMode(led2Pin, OUTPUT);

  // LEDs off initially
  digitalWrite(led1Pin, LOW);
  digitalWrite(led2Pin, LOW);

  // random client ID seed
  randomSeed(analogRead(A0));

  // PubSubClient setup
  mqttClient.setServer(broker, port);
  mqttClient.setCallback(mqttCallback);

  startWiFi();
}

void loop() {
  // keep connections alive
  maintainWiFi();
  maintainMQTT();

  // process incoming MQTT packets
  if (mqttClient.connected()) {
    mqttClient.loop();
  }

  detectGesture();
}

void startWiFi() {
  Serial.print("Connecting to wifi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  lastWiFiAttempt = millis();
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnectedOnce) {
      wifiConnectedOnce = true;
      Serial.println("Wifi connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }
    return;
  }

  // wifi dropped
  wifiConnectedOnce = false;

  // retry every few secs
  if (millis() - lastWiFiAttempt >= wifiRetryInterval) {
    Serial.println("Retrying wifi...");
    WiFi.begin(ssid, pass);
    lastWiFiAttempt = millis();
  }
}

void maintainMQTT() {
  // don't try MQTT if wifi is down
  if (WiFi.status() != WL_CONNECTED) return;

  if (mqttClient.connected()) return;

  if (millis() - lastMQTTAttempt < mqttRetryInterval) return;
  lastMQTTAttempt = millis();

  // generate random client ID
  String clientId = "nano33-" + String(random(0xFFFF), HEX);

  Serial.print("Connecting to broker ");
  Serial.println(broker);

  if (!mqttClient.connect(clientId.c_str())) {
    Serial.print("MQTT failed, rc = ");
    Serial.println(mqttClient.state());
    mqttConnectedOnce = false;
    return;
  }

  // successful connect
  mqttClient.subscribe(topicWave);
  mqttClient.subscribe(topicPat);

  mqttConnectedOnce = true;
  Serial.println("MQTT connected");
  Serial.print("Subscribed to: ");
  Serial.print(topicWave);
  Serial.print(" and ");
  Serial.println(topicPat);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String payloadStr = "";

  for (unsigned int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }

  Serial.print("Got msg on ");
  Serial.print(topicStr);
  Serial.print(": ");
  Serial.println(payloadStr);

  // turn LEDs on or off depending on topic
  if (topicStr == topicWave) {
    ledsOn();
    Serial.println("Turned both LEDs ON");
  }
  else if (topicStr == topicPat) {
    ledsOff();
    Serial.println("Turned both LEDs OFF");
  }
}

// simple helpers for LEDs
void ledsOn() {
  digitalWrite(led1Pin, HIGH);
  digitalWrite(led2Pin, HIGH);
}

void ledsOff() {
  digitalWrite(led1Pin, LOW);
  digitalWrite(led2Pin, LOW);
}

long readDistanceCm() {
  // ping the ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // timeout around 25ms so loop doesn't block too long
  long duration = pulseIn(echoPin, HIGH, 25000);
  if (duration == 0) return -1; // timed out

  float distance = duration * 0.0343f / 2.0f;
  return (long)distance;
}

void publishWave() {
  Serial.println("Wave detect -> publishing ES/Wave");

  if (mqttClient.connected()) {
    mqttClient.publish(topicWave, myName);
  }

  // trigger local LEDs so it feels instant
  ledsOn();
  Serial.println("Local LEDs turned ON");
}

void publishPat() {
  Serial.println("Pat detect -> publishing ES/Pat");

  if (mqttClient.connected()) {
    mqttClient.publish(topicPat, myName);
  }

  // trigger local LEDs instantly
  ledsOff();
  Serial.println("Local LEDs turned OFF");
}

void detectGesture() {
  // don't poll sensor too fast
  if (millis() - lastSampleAt < sampleInterval) return;
  lastSampleAt = millis();

  long distance = readDistanceCm();

  // print distance every half sec so monitor isn't spammed
  if (millis() - lastPrintAt >= printInterval) {
    Serial.print("Dist: ");
    Serial.println(distance);
    lastPrintAt = millis();
  }

  // cooldown after a gesture
  if (millis() - lastTriggerAt < cooldownMs) return;

  bool near = (distance > 0 && distance < gestureThresholdCm);

  // hand just entered zone
  if (near && !objectNear) {
    objectNear = true;
    nearStartedAt = millis();
  }

  // hand just left zone
  if (!near && objectNear) {
    objectNear = false;
    unsigned long duration = millis() - nearStartedAt;

    // wave: held hand there for a bit
    if (duration >= waveMinMs) {
      publishWave();
      quickHitCount = 0; // reset taps just in case
      lastTriggerAt = millis();
      return;
    }

    // pat: quick taps
    if (duration >= patMinMs && duration <= patMaxMs) {
      // first tap or too slow since last one
      if (quickHitCount == 0 || millis() - firstHitAt > patWindowMs) {
        quickHitCount = 1;
        firstHitAt = millis();
      } else {
        quickHitCount++;

        // got double tap
        if (quickHitCount >= 2) {
          publishPat();
          quickHitCount = 0;
          lastTriggerAt = millis();
          return;
        }
      }
    }
  }

  // reset pat window if second tap was too slow
  if (quickHitCount > 0 && millis() - firstHitAt > patWindowMs) {
    quickHitCount = 0;
  }
}