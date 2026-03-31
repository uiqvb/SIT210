#include <spi_flash.h>
#include <WiFiNINA.h>

char ssid[] = "";
char pass[] = "";

WiFiServer server(80);

const int LED_LIVING = 3;
const int LED_BATH = 5;
const int LED_CLOSET = 8;

bool livingroom = false; //leds set to OFF initially
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
  server.begin();

  Serial.println("Server started");
  Serial.print("Arduino IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  checkTimers();
  WiFiClient client = server.available();

  if (client) {
    String request = "";
    boolean currentLineIsBlank = true;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;

        if (c == '\n' && currentLineIsBlank) {
          handleRequest(client, request);
          break;
        }

        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }

    delay(1);
    client.stop();
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    delay(3000);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to WiFi");
}

void checkTimers() {
  unsigned long now = millis();

  if (livingOffAt != 0 && now >= livingOffAt) {
    livingroom = false;
    digitalWrite(LED_LIVING, LOW);
    livingOffAt = 0;
  }

  if (bathOffAt != 0 && now >= bathOffAt) {
    bathroom = false;
    digitalWrite(LED_BATH, LOW);
    bathOffAt = 0;
  }

  if (closetOffAt != 0 && now >= closetOffAt) {
    closet = false;
    digitalWrite(LED_CLOSET, LOW);
    closetOffAt = 0;
  }
}

void handleRequest(WiFiClient client, String request) {
  Serial.println("Incoming request:");
  Serial.println(request);

  if (request.indexOf("GET /toggle?room=livingroom") >= 0) {
    toggleRoom("livingroom");
    sendJson(client, "{\"success\":true,\"action\":\"toggle\",\"room\":\"livingroom\"}");
  }
  else if (request.indexOf("GET /toggle?room=bathroom") >= 0) {
    toggleRoom("bathroom");
    sendJson(client, "{\"success\":true,\"action\":\"toggle\",\"room\":\"bathroom\"}");
  }
  else if (request.indexOf("GET /toggle?room=closet") >= 0) {
    toggleRoom("closet");
    sendJson(client, "{\"success\":true,\"action\":\"toggle\",\"room\":\"closet\"}");
  }
  else if (request.indexOf("GET /timer?") >= 0) {
    String room = getQueryValue(request, "room");
    String secondsStr = getQueryValue(request, "seconds");
    int seconds = secondsStr.toInt();

    if ((room == "livingroom" || room == "bathroom" || room == "closet") && seconds > 0) {
      startTimer(room, seconds);
      sendJson(client, "{\"success\":true,\"action\":\"timer\"}");
    } else {
      sendJson(client, "{\"success\":false,\"error\":\"invalid timer request\"}");
    }
  }
  else if (request.indexOf("GET /status") >= 0) {
    String json = "{";
    json += "\"livingroom\":" + String(livingroom ? "true" : "false") + ",";
    json += "\"bathroom\":" + String(bathroom ? "true" : "false") + ",";
    json += "\"closet\":" + String(closet ? "true" : "false");
    json += "}";
    sendJson(client, json);
  }
  else {
    sendJson(client, "{\"success\":false,\"message\":\"unknown endpoint\"}");
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
}

String getQueryValue(String request, String key) {
  int qIndex = request.indexOf("?");
  if (qIndex == -1) return "";

  int endIndex = request.indexOf(" HTTP/");
  if (endIndex == -1) return "";

  String query = request.substring(qIndex + 1, endIndex);

  String pattern = key + "=";
  int keyIndex = query.indexOf(pattern);
  if (keyIndex == -1) return "";

  int valueStart = keyIndex + pattern.length();
  int ampIndex = query.indexOf("&", valueStart);

  if (ampIndex == -1) {
    return query.substring(valueStart);
  } else {
    return query.substring(valueStart, ampIndex);
  }
}

void sendJson(WiFiClient client, String json) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println("Access-Control-Allow-Origin: *");
  client.println();
  client.println(json);
}