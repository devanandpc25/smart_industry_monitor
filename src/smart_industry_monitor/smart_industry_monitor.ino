#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <TinyGPSPlus.h>
#include <WiFi.h>        // NEW: Wi-Fi for the dashboard
#include <WebServer.h>   // NEW: serves /data as JSON

// ========== WI-FI (NEW) ==========
// Use a 2.4 GHz network. The ESP32 does not support 5 GHz.
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
WebServer server(80);

// ========== PIN CONFIGURATION ==========
#define DHT_PIN       4
#define DHT_TYPE      DHT22
#define MQ_PIN        34

#define RED_LED       26
#define GREEN_LED     25

#define GPS_RX        16  // ESP32 receives from GPS TX
#define GPS_TX        17  // ESP32 transmits to GPS RX

// ========== DISPLAY CONFIGURATION ==========
#define LCD_ADDRESS   0x27
#define OLED_ADDRESS  0x3C

#define OLED_WIDTH    128
#define OLED_HEIGHT   64

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

Adafruit_SSD1306 oled(
  OLED_WIDTH, OLED_HEIGHT, &Wire, -1
);

// ========== SENSORS ==========
DHT dht(DHT_PIN, DHT_TYPE);
HardwareSerial gpsSerial(2);
TinyGPSPlus gps;

// ========== THRESHOLDS ==========
// EXAMPLE VALUES ONLY: calibrate before use.
// Keep these identical to the values at the top of dashboard/index.html.
const float MAX_TEMP_C        = 50.0;   // level 1: DANGER
const float MAX_TEMP_DISASTER = 60.0;   // level 2: DISASTER

// ESP32 ADC range is normally 0-4095.
// These thresholds are NOT calibrated gas concentrations.
const int GAS_THRESHOLD = 1800;         // level 1: DANGER
const int GAS_DISASTER  = 2600;         // level 2: DISASTER

// ========== LIVE SENSOR DATA ==========
float temperature = NAN;
float humidity = NAN;
int gasValue = 0;

bool sensorFault = true;
bool danger = true;
int  level = 0;   // 0 NORMAL, 1 DANGER, 2 DISASTER, 3 EVACUATE (gas AND temp at disaster)
bool redState = false;

// ========== TIMERS ==========
unsigned long lastSensorRead = 0;
unsigned long lastLCDUpdate = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastBlink = 0;
unsigned long lastWifiCheck = 0;

const unsigned long SENSOR_INTERVAL = 2000;
const unsigned long DISPLAY_INTERVAL = 2000;
const unsigned long BLINK_INTERVAL = 400;
const unsigned long WIFI_CHECK_INTERVAL = 15000;

// ========== DASHBOARD ENDPOINT (NEW) ==========
void handleData() {
  bool fix = gps.location.isValid() && gps.location.age() < 5000;

  String json = "{";
  json += "\"temperature\":" + (isnan(temperature) ? String("null") : String(temperature, 1));
  json += ",\"humidity\":"   + (isnan(humidity)    ? String("null") : String(humidity, 0));
  json += ",\"gas\":" + String(gasValue);
  json += ",\"level\":" + String(level);
  json += ",\"fault\":" + String(sensorFault ? "true" : "false");
  json += ",\"lat\":" + (fix ? String(gps.location.lat(), 6) : String("null"));
  json += ",\"lng\":" + (fix ? String(gps.location.lng(), 6) : String("null"));
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");  // lets the dashboard page read this
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wi-Fi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Dashboard address: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/data");
  } else {
    // The monitor keeps working without Wi-Fi. Only the dashboard needs it.
    Serial.println("Wi-Fi not connected. Local displays still work.");
  }
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  // ESP32 I2C pins
  Wire.begin(21, 22);

  dht.begin();

  // GPS hardware UART
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("INDUSTRIAL SAFE");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  // OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("OLED initialization failed!");
    while (true) {
      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, !digitalRead(RED_LED));
      delay(300);
    }
  }

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.println("INDUSTRIAL SAFETY");
  oled.println("Initializing...");
  oled.display();

  // NEW: Wi-Fi and web server
  connectWiFi();
  server.on("/data", handleData);
  server.begin();

  if (WiFi.status() == WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dashboard IP:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(3000);
  }

  // Initial sensor reading
  readSensors();
  updateSafety();

  updateLCD();
  updateOLED();

  lastSensorRead = millis();
  lastLCDUpdate = millis();
  lastOLEDUpdate = millis();

  Serial.println("System started");
}

// ========== MAIN LOOP ==========
void loop() {
  // Process GPS continuously
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  server.handleClient();  // NEW: answer dashboard requests

  unsigned long now = millis();

  // NEW: try to reconnect if Wi-Fi drops
  if (WiFi.status() != WL_CONNECTED && now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = now;
    WiFi.reconnect();
  }

  // Read sensors every 2 seconds
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    readSensors();
    updateSafety();

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" C | Humidity: ");
    Serial.print(humidity);
    Serial.print(" % | Gas ADC: ");
    Serial.print(gasValue);
    Serial.print(" | Level: ");
    Serial.print(level);
    Serial.print(" | Status: ");
    Serial.println(danger ? "DANGER" : "NORMAL");
  }

  // LED indication
  if (danger) {
    digitalWrite(GREEN_LED, LOW);

    if (now - lastBlink >= BLINK_INTERVAL) {
      lastBlink = now;
      redState = !redState;
      digitalWrite(RED_LED, redState ? HIGH : LOW);
    }
  } else {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    redState = false;
  }

  // Refresh displays
  if (now - lastLCDUpdate >= DISPLAY_INTERVAL) {
    lastLCDUpdate = now;
    updateLCD();
  }

  if (now - lastOLEDUpdate >= DISPLAY_INTERVAL) {
    lastOLEDUpdate = now;
    updateOLED();
  }
}

// ========== READ SENSORS ==========
void readSensors() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // ESP32 ADC
  gasValue = analogRead(MQ_PIN);

  temperature = t;
  humidity = h;
  sensorFault = (isnan(t) || isnan(h));
}

// ========== SAFETY DECISION ==========
void updateSafety() {
  bool tempKnown = !isnan(temperature);

  bool gasDanger    = gasValue >= GAS_THRESHOLD;
  bool gasDisaster  = gasValue >= GAS_DISASTER;
  bool tempDanger   = tempKnown && temperature >= MAX_TEMP_C;
  bool tempDisaster = tempKnown && temperature >= MAX_TEMP_DISASTER;

  if (gasDisaster && tempDisaster)      level = 3;  // EVACUATE
  else if (gasDisaster || tempDisaster) level = 2;  // DISASTER
  else if (gasDanger || tempDanger)     level = 1;  // DANGER
  else                                  level = 0;  // NORMAL

  danger = sensorFault || level > 0;

  if (danger) {
    digitalWrite(GREEN_LED, LOW);
  } else {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  }
}

// ========== LCD ==========
void updateLCD() {
  lcd.clear();

  if (danger) {
    lcd.setCursor(0, 0);

    if (sensorFault) {
      lcd.print("SENSOR FAULT!");
    } else if (level == 1) {
      lcd.print("DANGER / ALERT");
    } else if (level == 2) {
      lcd.print("!!! DISASTER !!!");
    } else {
      lcd.print("EVACUATE / ALERT");
    }

    lcd.setCursor(0, 1);
    lcd.print("G:");
    lcd.print(gasValue);
    lcd.print(" T:");

    if (isnan(temperature)) {
      lcd.print("--");
    } else {
      lcd.print(temperature, 1);
    }
    lcd.print("C");

  } else {
    lcd.setCursor(0, 0);
    lcd.print("NORMAL CONDITION");

    lcd.setCursor(0, 1);
    lcd.print("T:");

    lcd.print(temperature, 1);
    lcd.print("C H:");
    lcd.print(humidity, 0);
    lcd.print("%");
  }
}

// ========== OLED ==========
void updateOLED() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  // Line 1: status
  oled.setCursor(0, 0);

  if (sensorFault) {
    oled.println("STATUS: SENSOR ERR");
  } else if (level == 1) {
    oled.println("!! DANGER / ALERT !!");
  } else if (level == 2) {
    oled.println("!!! DISASTER !!!");
  } else if (level == 3) {
    oled.println("!! EVACUATE NOW !!");
  } else {
    oled.println("STATUS: NORMAL");
  }

  // Line 2: temperature, humidity
  oled.setCursor(0, 12);
  oled.print("T:");

  if (isnan(temperature)) {
    oled.print("--");
  } else {
    oled.print(temperature, 1);
  }

  oled.print("C H:");

  if (isnan(humidity)) {
    oled.print("--");
  } else {
    oled.print(humidity, 0);
  }

  oled.print("% G:");
  oled.print(gasValue);

  // Line 3 and 4: GPS location
  oled.setCursor(0, 26);

  if (gps.location.isValid() &&
      gps.location.age() < 5000) {
    oled.print("LAT:");
    oled.print(gps.location.lat(), 4);

    oled.setCursor(0, 38);
    oled.print("LON:");
    oled.print(gps.location.lng(), 4);
  } else {
    oled.print("GPS: WAITING FOR FIX");
    oled.setCursor(0, 38);
    oled.print("Location unavailable");
  }

  // Line 5: recommended action
  oled.setCursor(0, 52);

  if (sensorFault) {
    oled.print("CHECK SENSOR");
  } else if (level == 1) {
    oled.print("Check area");
  } else if (level == 2) {
    oled.print("ALERT SUPERVISOR");
  } else if (level == 3) {
    oled.print("EVACUATE / ALERT");
  } else {
    oled.print("Continue monitoring");
  }

  oled.display();
}
