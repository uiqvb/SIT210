#include <Wire.h>

// These are the pin connections I used in my final setup.
const byte PIR_PIN = 2;
const byte SWITCH_PIN = 4;
const byte LED1_PIN = 6;
const byte LED2_PIN = 8;

// This threshold is used to decide whether the room is dark enough
// for the automatic lighting to turn on.
const float DARK_THRESHOLD_LUX = 80.0;

// I read the BH1750 once every second instead of every single loop.
const unsigned long LUX_INTERVAL_MS = 1000;

// This keeps the LEDs on for a few seconds after motion stops so
// they do not turn off too suddenly.
const unsigned long MOTION_HOLD_MS = 5000;

// Simple debounce times to avoid false triggers from switch bounce
// or noisy PIR changes.
const unsigned long SWITCH_DEBOUNCE_MS = 80;
const unsigned long PIR_DEBOUNCE_MS = 150;

// BH1750 command for continuous high resolution mode.
const byte BH1750_MODE = 0x10;

// These flags are set inside the interrupt routines.
// I made them volatile because they can change outside the main loop.
volatile bool pirFlag = false;
volatile bool switchFlag = false;

// These variables keep track of the current system state.
bool manualSwitchOn = false;
bool pirHigh = false;
bool ledsOn = false;
bool bh1750Ready = false;
bool isDark = false;

// The BH1750 is usually on 0x23, but some modules use 0x5C.
uint8_t bh1750Addr = 0x23;

// Stores the last lux reading from the sensor.
float lastLux = -1.0;

// These store timestamps so I can use millis() for timing.
unsigned long lastLuxMs = 0;
unsigned long lastPirMs = 0;
unsigned long lastSwitchMs = 0;
unsigned long lastMotionMs = 0;

// The PIR interrupt just sets a flag.
// I kept it short on purpose because it is better not to do heavy work in an ISR.
void pirISR() {
  pirFlag = true;
}

// Same idea here for the slider switch interrupt.
void switchISR() {
  switchFlag = true;
}

// This checks whether an I2C device is present at a given address.
// I use it to detect the BH1750.
bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// This tries to start the BH1750.
// First it checks the common addresses, then sends the mode command.
bool initBH1750() {
  if (i2cPresent(0x23)) {
    bh1750Addr = 0x23;
  } else if (i2cPresent(0x5C)) {
    bh1750Addr = 0x5C;
  } else {
    return false;
  }

  Wire.beginTransmission(bh1750Addr);
  Wire.write(BH1750_MODE);
  return Wire.endTransmission() == 0;
}

// Reads the current lux value from the BH1750.
// If the read fails, it returns -1.0.
float readLux() {
  Wire.requestFrom((int)bh1750Addr, 2);
  if (Wire.available() == 2) {
    uint16_t raw = ((uint16_t)Wire.read() << 8) | Wire.read();
    return raw / 1.2;
  }
  return -1.0;
}

// Motion is considered active if the PIR is currently HIGH,
// or if motion happened recently and is still inside the hold time.
bool motionActive() {
  return pirHigh || (millis() - lastMotionMs < MOTION_HOLD_MS);
}

// This turns both LEDs on or off together.
void setLEDs(bool on) {
  digitalWrite(LED1_PIN, on ? HIGH : LOW);
  digitalWrite(LED2_PIN, on ? HIGH : LOW);
}

// This prints a detailed message so I can understand exactly
// why the LEDs changed state during testing.
void printLEDState(const char* source) {
  Serial.print("[LED] ");
  Serial.print(ledsOn ? "ON" : "OFF");
  Serial.print(" | Source: ");
  Serial.print(source);
  Serial.print(" | Manual=");
  Serial.print(manualSwitchOn ? "ON" : "OFF");
  Serial.print(" | Motion=");
  Serial.print(motionActive() ? "YES" : "NO");
  Serial.print(" | Lux=");
  Serial.print(lastLux, 1);
  Serial.print(" | ");
  Serial.println(isDark ? "DARK" : "BRIGHT");
}

// This is the main decision-making part for the LEDs.
// The LEDs should turn on if the manual switch is on,
// or if motion is detected while it is dark.
void updateLEDLogic(const char* source) {
  bool autoOn = bh1750Ready && isDark && motionActive();
  bool shouldOn = manualSwitchOn || autoOn;

  if (shouldOn != ledsOn) {
    ledsOn = shouldOn;
    setLEDs(ledsOn);
    printLEDState(source);
  }
}

// This checks the current state of the slider switch.
// In my setup, LOW means the manual backup is ON.
void checkSwitchState(const char* source) {
  unsigned long now = millis();
  bool current = (digitalRead(SWITCH_PIN) == LOW);

  if (current != manualSwitchOn && (now - lastSwitchMs >= SWITCH_DEBOUNCE_MS)) {
    lastSwitchMs = now;
    manualSwitchOn = current;

    Serial.print("[SWITCH] ");
    Serial.println(manualSwitchOn ? "Manual backup ON (D4 LOW)" : "Manual backup OFF (D4 HIGH)");

    updateLEDLogic(source);
  }
}

// This handles PIR state changes after the interrupt flag has been set.
void handlePirChange() {
  unsigned long now = millis();
  if (now - lastPirMs < PIR_DEBOUNCE_MS) return;
  lastPirMs = now;

  bool current = digitalRead(PIR_PIN);

  if (current != pirHigh) {
    pirHigh = current;

    if (pirHigh) {
      lastMotionMs = now;
      Serial.println("[PIR] Motion detected (D2 HIGH)");

      if (bh1750Ready) {
        if (isDark) {
          Serial.println("[AUTO] Motion + dark -> LEDs should turn ON");
        } else {
          Serial.println("[AUTO] Motion detected, but it is bright -> LEDs stay OFF unless switch is ON");
        }
      } else {
        Serial.println("[AUTO] BH1750 not ready -> auto dark-mode control unavailable");
      }
    } else {
      Serial.println("[PIR] PIR output LOW");
    }

    updateLEDLogic("PIR");
  }
}

// This reads the light sensor once per interval and updates
// the bright/dark state of the system.
void readLuxTask() {
  if (!bh1750Ready) return;

  unsigned long now = millis();
  if (now - lastLuxMs < LUX_INTERVAL_MS) return;
  lastLuxMs = now;

  float lux = readLux();
  if (lux < 0) {
    Serial.println("[BH1750] Read error");
    return;
  }

  lastLux = lux;
  bool newDark = (lastLux <= DARK_THRESHOLD_LUX);

  Serial.print("[LUX] ");
  Serial.print(lastLux, 1);
  Serial.print(" lx -> ");
  Serial.println(newDark ? "DARK" : "BRIGHT");

  if (newDark != isDark) {
    isDark = newDark;
    Serial.print("[AUTO] Darkness changed -> ");
    Serial.println(isDark ? "DARK mode active" : "BRIGHT mode active");
    updateLEDLogic("BH1750");
  }
}

void setup() {
  Serial.begin(115200);

  // This gives the Serial Monitor a short chance to connect
  // so the startup messages are easier to see.
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}

  pinMode(PIR_PIN, INPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  setLEDs(false);

  Wire.begin();
  bh1750Ready = initBH1750();

  // Read the startup states so the system begins in the correct condition.
  manualSwitchOn = (digitalRead(SWITCH_PIN) == LOW);
  pirHigh = digitalRead(PIR_PIN);

  if (pirHigh) {
    lastMotionMs = millis();
  }

  Serial.println("=== SIT210 Task 4.1P Final ===");
  Serial.println("[START] PIR=D2, SWITCH=D4, LED1=D6, LED2=D8, BH1750=A4/A5");
  Serial.print("[START] Switch startup state: ");
  Serial.println(manualSwitchOn ? "ON (LOW)" : "OFF (HIGH)");

  if (bh1750Ready) {
    // The BH1750 needs a short moment before the first reading.
    delay(180);

    lastLux = readLux();
    if (lastLux >= 0) {
      isDark = (lastLux <= DARK_THRESHOLD_LUX);
      Serial.print("[BH1750] Detected at 0x");
      Serial.println(bh1750Addr == 0x23 ? "23" : "5C");
      Serial.print("[LUX] ");
      Serial.print(lastLux, 1);
      Serial.print(" lx -> ");
      Serial.println(isDark ? "DARK" : "BRIGHT");
    } else {
      Serial.println("[BH1750] First read failed");
    }
  } else {
    Serial.println("[BH1750] Not detected");
  }

  // Both the PIR and the slider switch use interrupts in this task.
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), pirISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switchISR, CHANGE);

  updateLEDLogic("startup");
}

void loop() {
  bool pirEvent = false;
  bool swEvent = false;

  // I copy and clear the interrupt flags safely here so I do not
  // accidentally read them while they are changing.
  noInterrupts();
  pirEvent = pirFlag;
  swEvent = switchFlag;
  pirFlag = false;
  switchFlag = false;
  interrupts();

  if (pirEvent) {
    handlePirChange();
  }

  if (swEvent) {
    checkSwitchState("SWITCH interrupt");
  }

  // I left this extra polling check in as a safety backup
  // so the switch still works even if an interrupt edge is missed.
  checkSwitchState("SWITCH poll");

  readLuxTask();
  updateLEDLogic("loop");

  // Very small pause just to keep the loop from running too aggressively.
  delay(5);
}