#include <WiFiNINA.h>
#include <ThingSpeak.h>
#include <DHT.h>

// ---------------- Sensor pin setup ----------------
// DHT11 is connected to digital pin 2
#define DHTPIN 2
#define DHTTYPE DHT11

// Light sensor is connected to analogue pin A0
#define LIGHTPIN A0

// ---------------- Wi-Fi and ThingSpeak details ----------------
// Replace these my own Wi-Fi details
const char WIFI_SSID[] = "YOUR_WIFI_NAME";
const char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";

// Replace these my own ThingSpeak channel details
unsigned long channelID = 3301095;
const char WRITE_API_KEY[] = "YOUR_WRITE_API_KEY";

// This creates the Wi-Fi client object used by ThingSpeak
WiFiClient client;

// ---------------- Sensor object ----------------
// Create the DHT sensor object
DHT dht(DHTPIN, DHTTYPE);

// ---------------- Alarm thresholds ----------------
// These values decide when the system should trigger an alarm
const float TEMP_LOW_ALARM   = 18.0;
const float TEMP_HIGH_ALARM  = 30.0;
const int   LIGHT_LOW_ALARM  = 20;
const int   LIGHT_HIGH_ALARM = 2000;

// -------------------------------------------------
// Function: connectToWiFi
// Purpose: Connect the board to the Wi-Fi network
// -------------------------------------------------
void connectToWiFi() {
  Serial.print("Connecting to WiFi");

  // Keep trying until Wi-Fi connection is successful
  while (WiFi.begin(WIFI_SSID, WIFI_PASS) != WL_CONNECTED) {
    Serial.print(".");
    delay(5000);  // wait 5 seconds before trying again
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// -------------------------------------------------
// Function: readTemperatureC
// Purpose: Read temperature from the DHT11 in Celsius
// -------------------------------------------------
float readTemperatureC() {
  return dht.readTemperature();
}

// -------------------------------------------------
// Function: readHumidityPercent
// Purpose: Read humidity percentage from the DHT11
// -------------------------------------------------
float readHumidityPercent() {
  return dht.readHumidity();
}

// -------------------------------------------------
// Function: readLightRaw
// Purpose: Read the analogue light sensor value
// Notes:
// Instead of taking one reading, this takes 100 samples
// and averages them to reduce noise/fluctuation.
// -------------------------------------------------
int readLightRaw() {
  long total = 0;
  const int samples = 100;

  for (int i = 0; i < samples; i++) {
    total += analogRead(LIGHTPIN);
    delay(2);  // short delay between samples
  }

  return total / samples;
}

// -------------------------------------------------
// Function: calculateAlarmValue
// Purpose: Return 1 if temperature or light is outside
// the allowed range, otherwise return 0.
// -------------------------------------------------
int calculateAlarmValue(float temperature, int lightValue) {
  bool tempAlarm = (temperature < TEMP_LOW_ALARM) || (temperature > TEMP_HIGH_ALARM);
  bool lightAlarm = (lightValue < LIGHT_LOW_ALARM) || (lightValue > LIGHT_HIGH_ALARM);

  return (tempAlarm || lightAlarm) ? 1 : 0;
}

// -------------------------------------------------
// Function: buildAlarmMessage
// Purpose: Create a short text message describing
// the alarm condition for ThingSpeak status updates.
// -------------------------------------------------
String buildAlarmMessage(float temperature, int lightValue) {
  bool tempLow = temperature < TEMP_LOW_ALARM;
  bool tempHigh = temperature > TEMP_HIGH_ALARM;
  bool lightLow = lightValue < LIGHT_LOW_ALARM;
  bool lightHigh = lightValue > LIGHT_HIGH_ALARM;

  // If everything is within the normal range
  if (!tempLow && !tempHigh && !lightLow && !lightHigh) {
    return "NORMAL";
  }

  String msg = "ALARM: ";

  // Add temperature warning if needed
  if (tempLow) {
    msg += "Temp low ";
  } else if (tempHigh) {
    msg += "Temp high ";
  }

  // Add light warning if needed
  if (lightLow) {
    msg += "Light low ";
  } else if (lightHigh) {
    msg += "Light high ";
  }

  return msg;
}

// -------------------------------------------------
// Function: printReadings
// Purpose: Print all sensor readings and alarm info
// to the Serial Monitor for easy debugging.
// -------------------------------------------------
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

// -------------------------------------------------
// Function: uploadToThingSpeak
// Purpose: Send the latest sensor data to ThingSpeak
// so it can be viewed online.
// -------------------------------------------------
void uploadToThingSpeak(float temperature, float humidity, int lightValue, int alarmValue, const String& alarmMessage) {
  // Assign each reading to the correct ThingSpeak field
  ThingSpeak.setField(1, temperature);   // Field 1 = Temperature
  ThingSpeak.setField(2, lightValue);    // Field 2 = Light
  ThingSpeak.setField(3, alarmValue);    // Field 3 = Alarm
  ThingSpeak.setField(4, humidity);      // Field 4 = Humidity

  // Send an extra status message to ThingSpeak
  ThingSpeak.setStatus(alarmMessage);

  // Upload everything to the channel
  int responseCode = ThingSpeak.writeFields(channelID, WRITE_API_KEY);

  Serial.print("ThingSpeak response code: ");
  Serial.println(responseCode);

  // Response code 200 means the update worked
  if (responseCode == 200) {
    Serial.println("ThingSpeak update successful.");
  } else {
    Serial.println("ThingSpeak update failed.");
  }
}

// -------------------------------------------------
// Function: setup
// Purpose: Run once at startup to initialise Serial,
// sensors, Wi-Fi, and ThingSpeak.
// -------------------------------------------------
void setup() {
  Serial.begin(9600);

  // Wait for Serial Monitor to open
  while (!Serial) {}

  // Set analogue input resolution to 12 bits
  analogReadResolution(12);

  // Start the DHT11 sensor
  dht.begin();

  // Connect to Wi-Fi and initialise ThingSpeak
  connectToWiFi();
  ThingSpeak.begin(client);

  Serial.println("Starting temperature + humidity + light upload");
  Serial.println("-------------------------");
}

// -------------------------------------------------
// Function: loop
// Purpose: Repeats forever.
// 1. Check Wi-Fi
// 2. Read sensors
// 3. Calculate alarm
// 4. Print readings
// 5. Upload to ThingSpeak
// 6. Wait 30 seconds
// -------------------------------------------------
void loop() {
  // Reconnect if Wi-Fi drops out
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    connectToWiFi();
  }

  // Read all sensor values
  float temperature = readTemperatureC();
  float humidity = readHumidityPercent();
  int lightValue = readLightRaw();

  // Stop this cycle if DHT11 reading failed
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT11.");
    Serial.println("-------------------------");
    delay(30000);  // wait before trying again
    return;
  }

  // Work out whether the readings should trigger an alarm
  int alarmValue = calculateAlarmValue(temperature, lightValue);
  String alarmMessage = buildAlarmMessage(temperature, lightValue);

  // Show readings in Serial Monitor
  printReadings(temperature, humidity, lightValue, alarmValue, alarmMessage);

  // Send readings to ThingSpeak
  uploadToThingSpeak(temperature, humidity, lightValue, alarmValue, alarmMessage);

  // Wait 30 seconds before the next upload
  delay(30000);
}
