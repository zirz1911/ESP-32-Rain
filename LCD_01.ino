#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ─── WiFi Credentials ────────────────────────────────────────────────────────
#define WIFI_SSID "YOUR_SSID_HERE"
#define WIFI_PASS "YOUR_PASSWORD_HERE"

// ─── Hardware Pins ────────────────────────────────────────────────────────────
#define RAIN_DO  34
#define RAIN_AO  35

// ─── Objects ─────────────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);

// ─── Globals ──────────────────────────────────────────────────────────────────
int   g_analogVal   = 0;
int   g_digitalVal  = 0;
int   g_rainPercent = 0;
String g_status     = "Dry";
String g_ip         = "";

// ─── CORS helper ─────────────────────────────────────────────────────────────
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ─── Route: GET / ─────────────────────────────────────────────────────────────
void handleRoot() {
  addCORSHeaders();
  server.sendHeader("Location", "/data", true);
  server.send(302, "text/plain", "");
}

// ─── Route: GET /data ─────────────────────────────────────────────────────────
void handleData() {
  addCORSHeaders();

  StaticJsonDocument<256> doc;
  doc["ip"]           = g_ip;
  doc["rain_percent"] = g_rainPercent;
  doc["analog_val"]   = g_analogVal;
  doc["digital_val"]  = g_digitalVal;
  doc["status"]       = g_status;
  doc["uptime"]       = millis() / 1000;

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

// ─── Route: OPTIONS (preflight) ──────────────────────────────────────────────
void handleOptions() {
  addCORSHeaders();
  server.send(204, "text/plain", "");
}

// ─── WiFi connect ─────────────────────────────────────────────────────────────
void connectWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    g_ip = WiFi.localIP().toString();
    Serial.println("\nWiFi connected: " + g_ip);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(g_ip);
    delay(2000);
  } else {
    Serial.println("\nWiFi FAILED — running offline");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi FAILED");
    lcd.setCursor(0, 1);
    lcd.print("Offline mode");
    delay(2000);
    g_ip = "offline";
  }
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();
  pinMode(RAIN_DO, INPUT);

  // Splash screen
  lcd.setCursor(0, 0);
  lcd.print("Rain Sensor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();

  // WiFi
  connectWiFi();

  // Web server routes
  server.on("/",       HTTP_GET,     handleRoot);
  server.on("/data",   HTTP_GET,     handleData);
  server.on("/",       HTTP_OPTIONS, handleOptions);
  server.on("/data",   HTTP_OPTIONS, handleOptions);
  server.begin();

  Serial.println("HTTP server started on port 80");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Read sensors
  g_digitalVal  = digitalRead(RAIN_DO);
  g_analogVal   = analogRead(RAIN_AO);
  g_rainPercent = map(g_analogVal, 4095, 0, 0, 100);
  g_rainPercent = constrain(g_rainPercent, 0, 100);

  // Determine status
  if      (g_rainPercent < 10) g_status = "Dry";
  else if (g_rainPercent < 40) g_status = "Light Rain";
  else if (g_rainPercent < 70) g_status = "Moderate";
  else                         g_status = "Heavy Rain!";

  // LCD line 0: Rain %
  lcd.setCursor(0, 0);
  lcd.print("Rain: ");
  lcd.print(g_rainPercent);
  lcd.print("%   ");

  // LCD line 1: status (when offline) OR IP (when connected)
  lcd.setCursor(0, 1);
  if (g_ip == "offline" || g_ip == "") {
    // Pad to 16 chars to clear leftovers
    String statusPad = g_status;
    while (statusPad.length() < 16) statusPad += " ";
    lcd.print(statusPad);
  } else {
    // Show IP — shorter status on the right if room allows
    lcd.print(g_ip + "   ");
  }

  // Serial debug
  Serial.print("AO: ");    Serial.print(g_analogVal);
  Serial.print(" | %: ");  Serial.print(g_rainPercent);
  Serial.print(" | DO: "); Serial.print(g_digitalVal);
  Serial.print(" | St: "); Serial.println(g_status);

  delay(500);
}
