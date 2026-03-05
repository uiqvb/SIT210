const int porchLED = 3;
const int hallLED = 5;
const int switchPin = 8;

const unsigned long PORCH_TIME = 30000;
const unsigned long HALL_TIME  = 60000;

unsigned long sequenceStartTime = 0;
bool sequenceRunning = false;
bool sequenceComplete = false;

int lastPhysicalReading = HIGH;
int confirmedSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
    pinMode(porchLED, OUTPUT);
    pinMode(hallLED, OUTPUT);
    pinMode(switchPin, INPUT_PULLUP);
}

void loop() {
    int physicalReading = digitalRead(switchPin);

    if (physicalReading != lastPhysicalReading) {
        lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay) {
        confirmedSwitchState = physicalReading;
    }

    lastPhysicalReading = physicalReading;
    bool isSwitchOn = (confirmedSwitchState == LOW);

    if (isSwitchOn && !sequenceRunning && !sequenceComplete) {
        startLights();
        sequenceStartTime = millis();
        sequenceRunning = true;
    }

    if (!isSwitchOn) {
        turnOffEverything();
        sequenceRunning = false;
        sequenceComplete = false;
    }

    if (sequenceRunning) {
        unsigned long elapsedTime = millis() - sequenceStartTime;

        if (elapsedTime >= PORCH_TIME) {
            turnOffPorchLight();
        }

        if (elapsedTime >= HALL_TIME) {
            turnOffHallLight();
            sequenceRunning = false;
            sequenceComplete = true;
        }
    }
}

void startLights() {
    digitalWrite(porchLED, HIGH);
    digitalWrite(hallLED, HIGH);
}

void turnOffPorchLight() {
    digitalWrite(porchLED, LOW);
}

void turnOffHallLight() {
    digitalWrite(hallLED, LOW);
}

void turnOffEverything() {
    digitalWrite(porchLED, LOW);
    digitalWrite(hallLED, LOW);
}