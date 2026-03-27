//libaries
#include <Wire.h> // for i2c communication with the BH1750
#include <BH1750.h> // for the BH1750 light sensor
#include <WiFiNINA.h>// for wifi connectivity on the Nano 33 IoT
#include <PubSubClient.h> // for MQTT communication

//Configurations

// wifi credentials - not to be pushed to github
char ssid[] = "";
char pass[] = "";  // change this value before uplaoding to github

// mqtt server, port and topic, taken from mqtt in node
const char* mqttServer   = "";
const int   mqttPort     = ;
const char* mqttUser     = "";
const char* mqttPassword = "";
const char* mqttTopic    = "";


//Objects 
BH1750 lightMeter;
WiFiSSLClient wifiClient;
PubSubClient client(wifiClient);

// threshholds based on light when I shine a torch.
const float ON_THRESHOLD  = 1200.0;   
const float OFF_THRESHOLD = 800.0;    


//state variables
bool sunlightState = false;
bool prevPublished = false; 

// milis tracking variables 
unsigned long lastRead = 0;
unsigned long lastWifiTry = 0;
unsigned long lastMqttTry = 0;

const unsigned long READ_INTERVAL = 1000; 
const unsigned long RETRY_INTERVAL = 3000; // try reconnecting every 3s

void setup() {
  Serial.begin(9600); // start serial for debugging
  
  //   millis loop waiting for 1.5 seconds
  unsigned long startWait = millis();
  while(millis() - startWait < 1500) {
  }

  Wire.begin();

  // check if sensor is even pluged in
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) { // if light meter. bgin doesnt
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

<<<<<<< Updated upstream
      if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
=======
      if (client.connect(clientId.c_str(), mqttUser, mqttPassword))//sends unique client id and credentials to mqtt server{
>>>>>>> Stashed changes
        Serial.println("connected to broker!");
      } else {
        Serial.print("failed, rc="); // means return code
        Serial.println(client.state()); 
      }
      lastMqttTry = now;
    }
    return; // wait till we have mqtt to read the sensor
  }

  client.loop(); // keep mqtt alive if by sending a micro signal to the broker, also allows us to receive messages if we had any subscriptions (we dont in this case)

  // non-blocking sensor read
  if (now - lastRead >= READ_INTERVAL) { //has 1000 ms passed since last read?
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
        Serial.println("Published: SUNLIGHT_STARTED"); //published message to mqtt topic, this is what node red will react to
      } else {
        client.publish(mqttTopic, "SUNLIGHT_STOPPED");  //published message to mqtt topic, this is what node red will react to
        Serial.println("Published: SUNLIGHT_STOPED");
      }
      prevPublished = sunlightState;
    }
<<<<<<< Updated upstream
  }
}
=======
  }
>>>>>>> Stashed changes
