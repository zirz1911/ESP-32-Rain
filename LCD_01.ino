#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ─── WiFi Credentials ────────────────────────────────────────────────────────
#define WIFI_SSID "YOUR_SSID_HERE"
#define WIFI_PASS "YOUR_PASSWORD_HERE"

// ─── Hardware Pins ────────────────────────────────────────────────────────────
#define RAIN_DO   34
#define RAIN_AO   35
#define LED_RED   25
#define LED_BLUE  26
#define BTN_PIN   27

// ─── Objects ─────────────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);

// ─── Globals ──────────────────────────────────────────────────────────────────
int    g_analogVal   = 0;
int    g_digitalVal  = 0;
int    g_rainPercent = 0;
String g_status      = "Dry";
String g_ip          = "";

// LED state
bool g_ledRed  = false;
bool g_ledBlue = false;

// Mode: true = Auto, false = Manual
bool g_autoMode = true;

// Button debounce
bool     g_btnLastState    = HIGH;   // INPUT_PULLUP: idle = HIGH
bool     g_btnState        = HIGH;
unsigned long g_lastDebounce = 0;
const unsigned long DEBOUNCE_MS = 50;

// Blue LED blink (used during WiFi connecting phase)
unsigned long g_lastBlink     = 0;
bool          g_blinkState    = false;

// LCD mode-change message timer
unsigned long g_modeShowUntil = 0;

// ─── LED helper ───────────────────────────────────────────────────────────────
void applyLEDs() {
  digitalWrite(LED_RED,  g_ledRed  ? HIGH : LOW);
  digitalWrite(LED_BLUE, g_ledBlue ? HIGH : LOW);
}

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

  StaticJsonDocument<320> doc;
  doc["ip"]           = g_ip;
  doc["rain_percent"] = g_rainPercent;
  doc["analog_val"]   = g_analogVal;
  doc["digital_val"]  = g_digitalVal;
  doc["status"]       = g_status;
  doc["uptime"]       = millis() / 1000;
  doc["led_red"]      = g_ledRed;
  doc["led_blue"]     = g_ledBlue;
  doc["mode"]         = g_autoMode ? "auto" : "manual";

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

// ─── Route: GET /led ──────────────────────────────────────────────────────────
// Query params: red=0|1  blue=0|1
// Only takes effect in Manual Mode. Always returns current LED state + mode.
void handleLed() {
  addCORSHeaders();

  if (g_autoMode) {
    // Ignore the request but still return current state
    StaticJsonDocument<128> doc;
    doc["led_red"]  = g_ledRed;
    doc["led_blue"] = g_ledBlue;
    doc["mode"]     = "auto";
    doc["accepted"] = false;
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
    return;
  }

  if (server.hasArg("red")) {
    g_ledRed = server.arg("red") == "1";
  }
  if (server.hasArg("blue")) {
    g_ledBlue = server.arg("blue") == "1";
  }
  applyLEDs();

  StaticJsonDocument<128> doc;
  doc["led_red"]  = g_ledRed;
  doc["led_blue"] = g_ledBlue;
  doc["mode"]     = "manual";
  doc["accepted"] = true;
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

  // Blue LED blinks while connecting
  g_ledBlue = false;
  applyLEDs();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");

    // Blink blue LED while waiting
    g_ledBlue = !g_ledBlue;
    applyLEDs();

    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    g_ip = WiFi.localIP().toString();
    Serial.println("\nWiFi connected: " + g_ip);

    // Blue LED stays ON when connected (in auto mode)
    g_ledBlue = true;
    applyLEDs();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(g_ip);
    delay(2000);
  } else {
    Serial.println("\nWiFi FAILED — running offline");

    g_ledBlue = false;
    applyLEDs();

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
  pinMode(LED_RED,  OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(BTN_PIN,  INPUT_PULLUP);

  digitalWrite(LED_RED,  LOW);
  digitalWrite(LED_BLUE, LOW);

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
  server.on("/",     HTTP_GET,     handleRoot);
  server.on("/data", HTTP_GET,     handleData);
  server.on("/led",  HTTP_GET,     handleLed);
  server.on("/",     HTTP_OPTIONS, handleOptions);
  server.on("/data", HTTP_OPTIONS, handleOptions);
  server.on("/led",  HTTP_OPTIONS, handleOptions);
  server.begin();

  Serial.println("HTTP server started on port 80");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  // ── Button debounce ────────────────────────────────────────────────────────
  bool btnReading = digitalRead(BTN_PIN);
  if (btnReading != g_btnLastState) {
    g_lastDebounce = millis();
  }
  if ((millis() - g_lastDebounce) > DEBOUNCE_MS) {
    if (btnReading != g_btnState) {
      g_btnState = btnReading;
      // Trigger on falling edge (button pressed down, INPUT_PULLUP → LOW)
      if (g_btnState == LOW) {
        g_autoMode = !g_autoMode;
        Serial.println(g_autoMode ? "Mode: Auto" : "Mode: Manual");

        // Show mode on LCD for 2 seconds
        lcd.setCursor(0, 0);
        String modeLine = g_autoMode ? "Mode: Auto      " : "Mode: Manual    ";
        lcd.print(modeLine);
        g_modeShowUntil = millis() + 2000;
      }
    }
  }
  g_btnLastState = btnReading;

  // ── Read sensors ───────────────────────────────────────────────────────────
  g_digitalVal  = digitalRead(RAIN_DO);
  g_analogVal   = analogRead(RAIN_AO);
  g_rainPercent = map(g_analogVal, 4095, 0, 0, 100);
  g_rainPercent = constrain(g_rainPercent, 0, 100);

  // Determine status
  if      (g_rainPercent < 10) g_status = "Dry";
  else if (g_rainPercent < 40) g_status = "Light Rain";
  else if (g_rainPercent < 70) g_status = "Moderate";
  else                         g_status = "Heavy Rain!";

  // ── Auto Mode LED logic ─────────────────────────────────────────────────────
  if (g_autoMode) {
    bool rainDetected = (g_rainPercent >= 40 || g_digitalVal == 0);
    g_ledRed  = rainDetected;
    g_ledBlue = (WiFi.status() == WL_CONNECTED);
    applyLEDs();
  }

  // ── LCD update ─────────────────────────────────────────────────────────────
  bool showingMode = (millis() < g_modeShowUntil);

  if (!showingMode) {
    // LCD line 0: Rain %
    lcd.setCursor(0, 0);
    lcd.print("Rain: ");
    lcd.print(g_rainPercent);
    lcd.print("%   ");
  }

  // LCD line 1: status (offline) OR IP (connected)
  lcd.setCursor(0, 1);
  if (g_ip == "offline" || g_ip == "") {
    String statusPad = g_status;
    while (statusPad.length() < 16) statusPad += " ";
    lcd.print(statusPad);
  } else {
    lcd.print(g_ip + "   ");
  }

  // ── Serial debug ───────────────────────────────────────────────────────────
  Serial.print("AO: ");     Serial.print(g_analogVal);
  Serial.print(" | %: ");   Serial.print(g_rainPercent);
  Serial.print(" | DO: ");  Serial.print(g_digitalVal);
  Serial.print(" | St: ");  Serial.print(g_status);
  Serial.print(" | Mode: "); Serial.print(g_autoMode ? "Auto" : "Manual");
  Serial.print(" | R: ");   Serial.print(g_ledRed   ? "ON" : "off");
  Serial.print(" B: ");     Serial.println(g_ledBlue ? "ON" : "off");

  delay(500);
}
