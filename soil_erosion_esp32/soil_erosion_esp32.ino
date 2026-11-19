 
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// =====================================
// SMART IoT SOIL EROSION CONTROL SYSTEM
// =====================================

// SENSOR PINS
#define SOIL_PIN 34
#define RAIN_PIN 35

// LED PINS
#define GREEN_LED 19
#define YELLOW_LED 21
#define RED_LED 22

// BUZZER PIN
#define BUZZER_PIN 23

// SENSOR THRESHOLDS
const int soilThreshold = 2461;
const int rainThreshold = 3095;

// WIFI SETTINGS
const char* ssid = "Omkar";
const char* password = "YOUR_WIFI_PASSWORD";

// THINGSPEAK SETTINGS
String apiKey = "YOUR_NEW_THINGSPEAK_WRITE_API_KEY";

// TELEGRAM SETTINGS
String telegramBotToken = "YOUR_TELEGRAM_BOT_TOKEN";
String telegramChatID = "9199820779";

// TIMING
unsigned long lastUpload = 0;
const unsigned long uploadInterval = 20000;

unsigned long lastSirenChange = 0;
bool highTone = true;

// Track previous risk
int previousRiskLevel = -1;

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  pinMode(SOIL_PIN, INPUT);
  pinMode(RAIN_PIN, INPUT);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  allOutputsOff();

  analogReadResolution(12);

  ledcAttach(BUZZER_PIN, 2000, 8);
  ledcWriteTone(BUZZER_PIN, 0);

  Serial.println();
  Serial.println("================================");
  Serial.println(" SMART SOIL EROSION CONTROL");
  Serial.println("================================");

  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("Wi-Fi Connected!");

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    sendTelegramMessage(
      "Smart Soil Erosion Monitoring System is ONLINE!"
    );
  }
  else {
    Serial.println();
    Serial.println("Wi-Fi connection failed.");
    Serial.println("Local monitoring continues.");
  }
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  int soilValue = analogRead(SOIL_PIN);
  int rainValue = analogRead(RAIN_PIN);

  bool soilWet = soilValue < soilThreshold;
  bool raining = rainValue < rainThreshold;

  int riskLevel;

  if (soilWet && raining) {
    riskLevel = 2;
  }
  else if (soilWet || raining) {
    riskLevel = 1;
  }
  else {
    riskLevel = 0;
  }

  Serial.println();
  Serial.println("---------- SENSOR DATA ----------");

  Serial.print("Soil Moisture: ");
  Serial.println(soilValue);

  Serial.print("Rain Sensor: ");
  Serial.println(rainValue);

  Serial.print("Soil Condition: ");
  Serial.println(soilWet ? "WET" : "DRY");

  Serial.print("Rain Condition: ");
  Serial.println(raining ? "RAIN DETECTED" : "NO RAIN");

  // HIGH RISK
  if (riskLevel == 2) {

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, HIGH);

    Serial.println("EROSION RISK: HIGH");

    if (millis() - lastSirenChange >= 500) {

      if (highTone) {
        ledcWriteTone(BUZZER_PIN, 2000);
      }
      else {
        ledcWriteTone(BUZZER_PIN, 1000);
      }

      highTone = !highTone;
      lastSirenChange = millis();
    }
  }

  // MODERATE RISK
  else if (riskLevel == 1) {

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(RED_LED, LOW);

    ledcWriteTone(BUZZER_PIN, 0);

    Serial.println("EROSION RISK: MODERATE");
  }

  // LOW RISK
  else {

    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(RED_LED, LOW);

    ledcWriteTone(BUZZER_PIN, 0);

    Serial.println("EROSION RISK: LOW");
  }

  // TELEGRAM ALERT ON RISK CHANGE
  if (riskLevel == 2 && previousRiskLevel != 2) {

    String message = "HIGH EROSION RISK ALERT!\n";
    message += "Soil Moisture: " + String(soilValue) + "\n";
    message += "Rain Sensor: " + String(rainValue) + "\n";
    message += "Soil is WET and rain is detected.\n";
    message += "Immediate attention required!";

    sendTelegramMessage(message);
  }

  // TELEGRAM RECOVERY MESSAGE
  if (previousRiskLevel == 2 && riskLevel != 2) {

    String message = "Erosion risk has reduced.\n";
    message += "Current risk: ";

    if (riskLevel == 1) {
      message += "MODERATE";
    }
    else {
      message += "LOW";
    }

    sendTelegramMessage(message);
  }

  previousRiskLevel = riskLevel;

  Serial.println("---------------------------------");

  // THINGSPEAK UPLOAD
  if (millis() - lastUpload >= uploadInterval) {

    uploadToThingSpeak(soilValue, rainValue, riskLevel);

    lastUpload = millis();
  }

  delay(1000);
}

// =====================================
// TURN OFF OUTPUTS
// =====================================

void allOutputsOff() {

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);

  ledcWriteTone(BUZZER_PIN, 0);
}

// =====================================
// URL ENCODING
// =====================================

String urlEncode(String message) {

  String encoded = "";

  char hex[] = "0123456789ABCDEF";

  for (int i = 0; i < message.length(); i++) {

    unsigned char c = message.charAt(i);

    if (isalnum(c) || c == '-' || c == '_' ||
        c == '.' || c == '~') {

      encoded += (char)c;
    }
    else {

      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0F];
    }
  }

  return encoded;
}

// =====================================
// TELEGRAM MESSAGE FUNCTION
// =====================================

void sendTelegramMessage(String message) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Telegram skipped: Wi-Fi disconnected.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = "https://api.telegram.org/bot";
  url += telegramBotToken;
  url += "/sendMessage?chat_id=";
  url += telegramChatID;
  url += "&text=";
  url += urlEncode(message);

  http.setTimeout(15000);

  Serial.println("Sending Telegram message...");

  if (!http.begin(client, url)) {
    Serial.println("Telegram connection initialization failed.");
    return;
  }

  int responseCode = http.GET();

  if (responseCode > 0) {

    Serial.print("Telegram response: ");
    Serial.println(responseCode);

    Serial.println(http.getString());
  }
  else {

    Serial.print("Telegram error: ");
    Serial.println(http.errorToString(responseCode));
  }

  http.end();
}

// =====================================
// THINGSPEAK UPLOAD
// =====================================

void uploadToThingSpeak(int soil, int rain, int risk) {

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi unavailable. Upload skipped.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = "https://api.thingspeak.com/update?api_key=";
  url += apiKey;
  url += "&field1=" + String(soil);
  url += "&field2=" + String(rain);
  url += "&field3=" + String(risk);

  Serial.println("Sending data to ThingSpeak...");

  http.setTimeout(15000);

  if (!http.begin(client, url)) {
    Serial.println("ThingSpeak connection initialization failed.");
    return;
  }

  int responseCode = http.GET();

  if (responseCode > 0) {

    Serial.print("ThingSpeak response: ");
    Serial.println(responseCode);

    String response = http.getString();

    Serial.print("Server response: ");
    Serial.println(response);

    if (responseCode == 200 && response.toInt() > 0) {
      Serial.println("DATA UPLOADED SUCCESSFULLY!");
    }
    else {
      Serial.println("ThingSpeak update unsuccessful.");
    }
  }
  else {

    Serial.print("Upload error: ");
    Serial.println(responseCode);

    Serial.println(http.errorToString(responseCode));
  }

  http.end();
}
