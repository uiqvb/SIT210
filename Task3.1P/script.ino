#include <Wire.h>
#include <BH1750.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>

// wifi credentials - not to be pushed to github
char ssid[] = "S23 FE";
char pass[] = "manit1234";  // change this value before uplaoding to github

// mqtt server, port and topic, taken from mqtt in node
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* mqttTopic = "terrarium/S23FE/lightStatus";

BH1750 lightMeter;
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// threshholds based on light when I shine a torch.
const float ON_THRESHOLD  = 1200.0;   
const float OFF_THRESHOLD = 800.0;    

bool sunlightState = false;
bool prevPublished = false; 

// milis tracking variables 
unsigned long lastRead = 0;
unsigned long lastWifiTry = 0;
unsigned long lastMqttTry = 0;

const unsigned long READ_INTERVAL = 1000; 
const unsigned long RETRY_INTERVAL = 3000; // try reconnecting every 3s

void setup() {
  Serial.begin(9600);
  
  //   millis loop 
  unsigned long startWait = millis();
  while(millis() - startWait < 1500) {
  }

  Wire.begin();

  // check if sensor is even pluged in
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 missing or broke");
    while (1); // halt the program if the BH1750 is not detected
  }
  
  Serial.println("sensor ready");

  randomSeed(analogRead(A0));
  client.setServer(mqttServer, mqttPort);

  Serial.println("setup done. handling connections in the main loop now");
}

void loop() {
  unsigned long now = millis();

  // non-blocking wifi reconnect
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiTry >= RETRY_INTERVAL) {
      Serial.println("wifi droped or not connected. trying again...");
      WiFi.begin(ssid, pass);
      lastWifiTry = now;
    }
    return; // pause loop here so we dont do mqtt stuff offline
  }

  // non-blocking mqtt reconnect
  if (!client.connected()) {
    if (now - lastMqttTry >= RETRY_INTERVAL) {
      Serial.print("Wating for MQTT..."); 

      String clientId = "Nano33IoT-";
      clientId += String(random(1000, 9999));

      if (client.connect(clientId.c_str())) {
        Serial.println("connected to broker!");
      } else {
        Serial.print("failed, rc=");
        Serial.println(client.state()); 
      }
      lastMqttTry = now;
    }
    return; // wait till we have mqtt to read the sensor
  }

  client.loop(); // keep mqtt alive

  // non-blocking sensor read
  if (now - lastRead >= READ_INTERVAL) {
    lastRead = now;

    float lux = lightMeter.readLightLevel();

    if (!sunlightState && lux >= ON_THRESHOLD) {
      sunlightState = true;
    } else if (sunlightState && lux <= OFF_THRESHOLD) {
      sunlightState = false;
    }

    Serial.print("Lux: ");
    Serial.print(lux);
    Serial.print(" | State: ");
    Serial.println(sunlightState ? "SUNLIGHT" : "NO SUN");

    if (sunlightState != prevPublished) {
      if (sunlightState) {
        client.publish(mqttTopic, "SUNLIGHT_STARTED");
        Serial.println("Published: SUNLIGHT_STARTED");
      } else {
        client.publish(mqttTopic, "SUNLIGHT_STOPPED"); 
        Serial.println("Published: SUNLIGHT_STOPED");
      }
      prevPublished = sunlightState;
    }
  }
}