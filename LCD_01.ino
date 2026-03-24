#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define RAIN_DO  34   // Digital output
#define RAIN_AO  35   // Analog output

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  pinMode(RAIN_DO, INPUT);

  lcd.setCursor(0, 0);
  lcd.print("Rain Sensor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();
}

void loop() {
  int digitalVal = digitalRead(RAIN_DO);  // 0 = มีฝน, 1 = แห้ง
  int analogVal  = analogRead(RAIN_AO);   // 0–4095 (4095 = แห้งสนิท)

  // แปลงเป็น % (0% = แห้ง, 100% = เปียกมาก)
  int rainPercent = map(analogVal, 4095, 0, 0, 100);
  rainPercent = constrain(rainPercent, 0, 100);

  // กำหนดสถานะ
  String status;
  if (rainPercent < 10)       status = "Dry         ";
  else if (rainPercent < 40)  status = "Light Rain  ";
  else if (rainPercent < 70)  status = "Moderate    ";
  else                        status = "Heavy Rain! ";

  // แสดงผลบนจอ
  lcd.setCursor(0, 0);
  lcd.print("Rain: ");
  lcd.print(rainPercent);
  lcd.print("%   ");

  lcd.setCursor(0, 1);
  lcd.print(status);

  // Debug serial
  Serial.print("AO: "); Serial.print(analogVal);
  Serial.print(" | %: "); Serial.print(rainPercent);
  Serial.print(" | DO: "); Serial.println(digitalVal);

  delay(500);
}