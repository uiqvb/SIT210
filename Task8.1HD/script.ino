#include <ArduinoBLE.h>

BLEService lightService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEStringCharacteristic commandChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLEWrite | BLEWriteWithoutResponse, 20);

#define BATHROOM_LED 2
#define HALLWAY_LED  3
#define FAN_LED      4
#define LDR_PIN      A0
#define DARK_THRESHOLD 1020

void setup() {
  Serial.begin(9600);
  delay(3000);

  pinMode(BATHROOM_LED, OUTPUT);
  pinMode(HALLWAY_LED, OUTPUT);
  pinMode(FAN_LED, OUTPUT);

  if (!BLE.begin()) {
    while (1);
  }

  BLE.setLocalName("LightControl");
  BLE.setAdvertisedService(lightService);
  lightService.addCharacteristic(commandChar);
  BLE.addService(lightService);
  BLE.advertise();
}
void loop() {
  BLEDevice central = BLE.central();

  if (central) {
    while (central.connected()) {
      if (commandChar.written()) {
        String cmd = commandChar.value();
        if (cmd == "LIGHTS_ON") {
          int light = analogRead(LDR_PIN);
          if (light < DARK_THRESHOLD) {
            digitalWrite(BATHROOM_LED, HIGH);
            digitalWrite(HALLWAY_LED, HIGH);
            digitalWrite(FAN_LED, HIGH);
          }
        }
        if (cmd == "LIGHTS_OFF") {
          digitalWrite(BATHROOM_LED, LOW);
          digitalWrite(HALLWAY_LED, LOW);
          digitalWrite(FAN_LED, LOW);
        }
      }
    }
    BLE.advertise(); // ← re-advertise after disconnect
  }
}
