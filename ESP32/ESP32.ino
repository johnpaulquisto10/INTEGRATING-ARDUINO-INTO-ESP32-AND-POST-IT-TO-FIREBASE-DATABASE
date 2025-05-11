#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <time.h>

// WiFi credentials
const char* ssid = "johnpaulquisto";  
const char* password = "1010101010";  

// Firestore endpoint (project ID corrected)
const char* firestoreUrl = "https://firestore.googleapis.com/v1/projects/esp32-35e0e/databases/(default)/documents/sensorData";

// NTP Server settings for timestamp
const char* ntpServer = "time.google.com";
const long gmtOffset_sec = 8 * 3600; // GMT+8
const int daylightOffset_sec = 0;

// DHT Sensor settings
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// LDR settings
#define LDRPIN 35
#define THRESHOLD 1000

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected");
    Serial.println(WiFi.localIP());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  } else {
    Serial.println("\nWiFi Connection Failed! Restarting...");
    ESP.restart();
  }

  dht.begin();
  pinMode(LDRPIN, INPUT);
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
  }
}

void loop() {
  checkWiFi();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int ldrValue = analogRead(LDRPIN);
  String isDaylight = (ldrValue < THRESHOLD) ? "0" : "1";

  Serial.printf("LDR: %d => Daylight: %s\n", ldrValue, isDaylight.c_str());

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor");
    delay(2000);
    return;
  }

  Serial.printf("Temperature: %.2f°C\nHumidity: %.2f%%\n", temperature, humidity);

  char timestamp[30] = "Unknown";
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    Serial.print("Timestamp: ");
    Serial.println(timestamp);
  }

  sendToFirestore(temperature, humidity, isDaylight, timestamp);
  delay(5000);
}

void sendToFirestore(float temperature, float humidity, String daylight, const char* timestamp) {
  HTTPClient http;
  http.begin(firestoreUrl);
  http.addHeader("Content-Type", "application/json");

  String jsonData = "{"
    "\"fields\": {"
      "\"temperature\": {\"doubleValue\": " + String(temperature) + "},"
      "\"humidity\": {\"doubleValue\": " + String(humidity) + "},"
      "\"daylight\": {\"stringValue\": \"" + daylight + "\"},"
      "\"timestamp\": {\"stringValue\": \"" + String(timestamp) + "\"}"
    "}"
  "}";

  int httpResponseCode = http.POST(jsonData);
  Serial.print("Response Code: ");
  Serial.println(httpResponseCode);

  if (httpResponseCode > 0) {
    Serial.println("Response: " + http.getString());
  } else {
    Serial.println("Error: " + http.errorToString(httpResponseCode));
  }

  http.end();
}

