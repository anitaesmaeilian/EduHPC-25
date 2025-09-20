#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define SOIL_PIN 34
#define BME280_ADDRESS 0x76

const char* ssid = "..."; //WiFi Name
const char* password = "..."; //WiFi Password
const char* scriptURL = "..."; //The google sheets URL

Adafruit_BME280 bme;
BH1750 lightMeter;

bool bmeInitialized = false;
bool lightInitialized = false;

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 3 * 60 * 1000UL; // 15 minutes

void setup() {
  Serial.begin(115200);
  delay(1000); // Shorter delay

  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize BME280
  bmeInitialized = bme.begin(BME280_ADDRESS);
  Serial.println(bmeInitialized ? "✅ BME280 initialized." : "❌ BME280 not found.");

  delay(300);  // Let I2C bus settle

  // Initialize BH1750 only if needed
  lightInitialized = lightMeter.begin();
  Serial.println(lightInitialized ? "✅ BH1750 initialized." : "❌ BH1750 not found.");

  connectToWiFi();

  lastSendTime = millis() - sendInterval; // Send immediately
}

void loop() {
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();

    float temp = NAN, humidity = NAN, pressure = NAN, lux = NAN;

    if (bmeInitialized) {
      temp = bme.readTemperature();
      humidity = bme.readHumidity();
      pressure = bme.readPressure() / 100.0F;
    }

    if (lightInitialized) {
      lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
      delay(180);
      lux = lightMeter.readLightLevel();
    }

    int rawSoil = analogRead(SOIL_PIN);
    int moisturePercent = map(rawSoil, 2580, 1500, 0, 100);
    moisturePercent = constrain(moisturePercent, 0, 100);

    if (WiFi.status() != WL_CONNECTED) {
      connectToWiFi();
    }

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(scriptURL);
      http.addHeader("Content-Type", "application/json");

      String json = "{";
      json += "\"deviceId\":\"esp32_2\",";
      json += "\"temp\":" + String(temp, 1) + ",";
      json += "\"humidity\":" + String(humidity, 1) + ",";
      json += "\"pressure\":" + String(pressure, 1) + ",";
      json += "\"soil\":" + String(moisturePercent) + ",";
      json += "\"light\":" + String(lux, 1);
      json += "}";

      int responseCode = http.POST(json);
      String response = http.getString();

      Serial.printf("📤 HTTP Response Code: %d\n", responseCode);
      Serial.println("📨 Response: " + response);

      http.end();

      // Optional: Disconnect WiFi after sending
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    } else {
      Serial.println("❌ Wi-Fi not connected.");
    }
  }

  delay(100); // Minimal delay
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to Wi-Fi");
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Wi-Fi connect timeout.");
  }
}
