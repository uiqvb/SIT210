#include <WiFiNINA.h>
#include <ThingSpeak.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
#define LIGHTPIN A0

// ---------------- WiFi / ThingSpeak ----------------
const char WIFI_SSID[] = "S23 FE";
const char WIFI_PASS[] = "manit1234";

unsigned long channelID = 3301095;
const char WRITE_API_KEY[] = "UOZLKI3IMHVBKHET";

WiFiClient client;

// ---------------- Sensors ----------------
DHT dht(DHTPIN, DHTTYPE);

// ---------------- Alarm thresholds ----------------
const float TEMP_LOW_ALARM   = 18.0;
const float TEMP_HIGH_ALARM  = 30.0;
const int   LIGHT_LOW_ALARM  = 20;
const int   LIGHT_HIGH_ALARM = 2000;

// ---------------------------------------------------

void connectToWiFi() {
  Serial.print("Connecting to WiFi");

  while (WiFi.begin(WIFI_SSID, WIFI_PASS) != WL_CONNECTED) {
    Serial.print(".");
    delay(5000);
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

float readTemperatureC() {
  return dht.readTemperature();
}

float readHumidityPercent() {
  return dht.readHumidity();
}

int readLightRaw() {
  long total = 0;
  const int samples = 100;

  for (int i = 0; i < samples; i++) {
    total += analogRead(LIGHTPIN);
    delay(2);
  }

  return total / samples;
}

int calculateAlarmValue(float temperature, int lightValue) {
  bool tempAlarm = (temperature < TEMP_LOW_ALARM) || (temperature > TEMP_HIGH_ALARM);
  bool lightAlarm = (lightValue < LIGHT_LOW_ALARM) || (lightValue > LIGHT_HIGH_ALARM);

  return (tempAlarm || lightAlarm) ? 1 : 0;
}

String buildAlarmMessage(float temperature, int lightValue) {
  bool tempLow = temperature < TEMP_LOW_ALARM;
  bool tempHigh = temperature > TEMP_HIGH_ALARM;
  bool lightLow = lightValue < LIGHT_LOW_ALARM;
  bool lightHigh = lightValue > LIGHT_HIGH_ALARM;

  if (!tempLow && !tempHigh && !lightLow && !lightHigh) {
    return "NORMAL";
  }

  String msg = "ALARM: ";

  if (tempLow) {
    msg += "Temp low ";
  } else if (tempHigh) {
    msg += "Temp high ";
  }

  if (lightLow) {
    msg += "Light low ";
  } else if (lightHigh) {
    msg += "Light high ";
  }

  return msg;
}

void printReadings(float temperature, float humidity, int lightValue, int alarmValue, const String& alarmMessage) {
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Light raw: ");
  Serial.println(lightValue);

  Serial.print("Alarm field: ");
  Serial.println(alarmValue);

  Serial.print("Alarm status: ");
  Serial.println(alarmMessage);

  Serial.println("-------------------------");
}

void uploadToThingSpeak(float temperature, float humidity, int lightValue, int alarmValue, const String& alarmMessage) {
  ThingSpeak.setField(1, temperature);   // Field 1 = Temperature
  ThingSpeak.setField(2, lightValue);    // Field 2 = Light
  ThingSpeak.setField(3, alarmValue);    // Field 3 = Alarm
  ThingSpeak.setField(4, humidity);      // Field 4 = Humidity
  ThingSpeak.setStatus(alarmMessage);

  int responseCode = ThingSpeak.writeFields(channelID, WRITE_API_KEY);

  Serial.print("ThingSpeak response code: ");
  Serial.println(responseCode);

  if (responseCode == 200) {
    Serial.println("ThingSpeak update successful.");
  } else {
    Serial.println("ThingSpeak update failed.");
  }
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  analogReadResolution(12);
  dht.begin();

  connectToWiFi();
  ThingSpeak.begin(client);

  Serial.println("Starting temperature + humidity + light upload");
  Serial.println("-------------------------");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectToWiFi();
  }

  float temperature = readTemperatureC();
  float humidity = readHumidityPercent();
  int lightValue = readLightRaw();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT11.");
    Serial.println("-------------------------");
    delay(30000);
    return;
  }

  int alarmValue = calculateAlarmValue(temperature, lightValue);
  String alarmMessage = buildAlarmMessage(temperature, lightValue);

  printReadings(temperature, humidity, lightValue, alarmValue, alarmMessage);
  uploadToThingSpeak(temperature, humidity, lightValue, alarmValue, alarmMessage);

  delay(30000);
}