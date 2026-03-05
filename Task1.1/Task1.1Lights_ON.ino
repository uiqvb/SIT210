    const int porchLED = 3;
    const int hallLED = 5;
    const int switchPin = 8;

    const int PORCH_TIME = 30000;
    const int HALL_TIME  = 60000;

    bool triggered = false;

    void setup() {
    setupPins();
    }

    void loop() {

    if (checkSwitch() && !triggered) {
        activateLighting();
        triggered = true;
    }

    if (!checkSwitch()) {
        triggered = false;
    }
    }

    void setupPins() {
    pinMode(porchLED, OUTPUT);
    pinMode(hallLED, OUTPUT);
    pinMode(switchPin, INPUT_PULLUP);
    }

    bool checkSwitch() {
    return digitalRead(switchPin) == LOW ;//?
    }

    void activateLighting() {

    startLights();

    delay(PORCH_TIME);
    turnOffPorchLight();

    delay(HALL_TIME - PORCH_TIME);
    turnOffHallLight();
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