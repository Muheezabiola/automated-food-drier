#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include "DHT.h"
#include <time.h>

// ---------------- WiFi & Firebase (use your credentials) ----------------
// NOTE: the real credentials this sketch shipped with have been
// replaced with placeholders, because this copy is committed. The
// working values now live in include/secrets.h, which is git-ignored.
#define WIFI_SSID "your-network"
#define WIFI_PASSWORD "your-password"

#define API_KEY "your-web-api-key"
#define DATABASE_URL "https://your-project-default-rtdb.firebaseio.com"
#define USER_EMAIL "drier@example.com"
#define USER_PASSWORD "your-firebase-user-password"

const char *FB_BASE = "/drier/";

// ---------------- Hardware pins ----------------
#define DHTPIN 0
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define relayPIN1 16  // heat bit 0 (LSB)
#define relayPIN2 17  // heat bit 1
#define relayPIN3 18  // heat bit 2 (or used as levels)
#define relayPIN4 19  // fan relay

// LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);  // 20x4 LCD

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 13, 12, 14, 27 };
byte colPins[COLS] = { 26, 25, 33, 32 };
Keypad keypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

// ---------------- Firebase objects ----------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ---------------- State variables ----------------
enum Mode { MODE_IDLE,
            AUTO,
            MANUAL };
Mode currentMode = AUTO;    // start in AUTO
bool isOnline = false;      // true when firebase ready
bool forceOffline = false;  // set by '#' key => force offline
unsigned long lastFirebaseAttempt = 0;
const unsigned long FIREBASE_RETRY_MS = 10000UL;

unsigned long previousDHT = 0;
const unsigned long DHT_INTERVAL = 2000UL;
unsigned long lastUpload = 0;
const unsigned long UPLOAD_INTERVAL = 5000UL;

float currentTemp = 0.0;
float currentHum = 0.0;

int heatLevel = 0;  // 0..3 (0 => off)
bool fanState = false;

// Key handling
char lastKey = '\0';
unsigned long lastKeyTime = 0;
const unsigned long KEY_DISPLAY_TIMEOUT = 3000UL;  // 3 seconds

// ---------------- Helper forward declarations ----------------
void Heat_off();
void Heat_Level1();
void Heat_Level2();
void Heat_Level3();
void Fan_on();
void Fan_off();
void applyHeatLevel(int level);
void updateAutoMode();
void publishToFirebase();
void readCommandsFromFirebase();
void setupFirebaseAndTime();

// ---------------- Setup ----------------
void setup() {
  pinMode(relayPIN1, OUTPUT);
  pinMode(relayPIN2, OUTPUT);
  pinMode(relayPIN3, OUTPUT);
  pinMode(relayPIN4, OUTPUT);

  Serial.begin(115200);
  delay(50);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(500);

  dht.begin();

  // Default: heat off, fan off
  heatLevel = 0;
  Fan_off();
  applyHeatLevel(0);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   FOOD DRIER    ");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("WiFi connection attempt started...");
  lastUpload = millis();
  lastFirebaseAttempt = millis();
}

// ---------------- Loop ----------------
void loop() {
  bool isWifiConnected = (WiFi.status() == WL_CONNECTED);

  if (isWifiConnected && !isOnline && !forceOffline) {
    if (!Firebase.ready() && millis() - lastFirebaseAttempt > FIREBASE_RETRY_MS) {
      lastFirebaseAttempt = millis();
      setupFirebaseAndTime();
    }
    if (Firebase.ready()) {
      isOnline = true;
      Serial.println("✅ Firebase is ready.");
    }
  } else if ((!isWifiConnected || forceOffline) && isOnline) {
    isOnline = false;
    Serial.println("❌ Offline.");
  }

  // ---------- keypad handling ----------
  char key = keypad.getKey();
  if (key) {
    lastKey = key;
    lastKeyTime = millis();
    Serial.print("Key Pressed:");
    Serial.println(key);

    if (key == '#') {
      forceOffline = true;
    } else if (key == '*') {
      forceOffline = false;
    } else if (key == 'A') {
      currentMode = AUTO;
      Fan_on();
      if (isOnline) Firebase.setString(fbdo, String(FB_BASE) + "mode", "Auto");
    } else if (key == 'B') {
      currentMode = MANUAL;
      if (isOnline) Firebase.setString(fbdo, String(FB_BASE) + "mode", "Manual");
    } else if (currentMode == MANUAL) {
      if (key >= '0' && key <= '3') {
        heatLevel = key - '0';
        applyHeatLevel(heatLevel);
        if (isOnline) Firebase.setInt(fbdo, String(FB_BASE) + "heatLevel", heatLevel);
      } else if (key == 'C') {
        Fan_on();
        if (isOnline) Firebase.setBool(fbdo, String(FB_BASE) + "fanState", true);
      } else if (key == 'D') {
        Fan_off();
        if (isOnline) Firebase.setBool(fbdo, String(FB_BASE) + "fanState", false);
      }
    }
  }

  // ---------- DHT readings ----------
  if (millis() - previousDHT >= DHT_INTERVAL) {
    previousDHT = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHum = h;
    }
  }

  if (isOnline) readCommandsFromFirebase();

  // ---------- Mode logic ----------
  if (currentMode == AUTO) updateAutoMode();
  else {
    applyHeatLevel(heatLevel);
    if (fanState) Fan_on();
    else Fan_off();
  }

  // ====================================================
  //                 LCD DISPLAY (No Flicker)
  // ====================================================

  // Row 0: permanent banner
  lcd.setCursor(0, 0);
  lcd.print("    FOOD DRIER    ");

  // Row 1: Online status & mode
  lcd.setCursor(0, 1);
  lcd.print(isOnline ? " ONLINE " : " OFFLINE");
  lcd.print(currentMode == AUTO ? " | AUTO   " : " | MANUAL ");
  lcd.print("      ");  // padding

  // Row 2: Heat and fan state
  lcd.setCursor(0, 2);
  lcd.print("Heat:");
  lcd.print(heatLevel);
  lcd.print("  Fan:");
  lcd.print(fanState ? "ON " : "OFF");
  lcd.print("      ");

  // Row 3: Temp, Humidity, Key
  lcd.setCursor(0, 3);
  lcd.print("T:");
  lcd.print(currentTemp, 1);
  lcd.print("C H:");
  lcd.print(currentHum, 1);
  lcd.print("% K:");
  if (lastKey && (millis() - lastKeyTime <= KEY_DISPLAY_TIMEOUT)) {
    lcd.print(lastKey);
  } else {
    lcd.print('-');
  }
  lcd.print("     ");  // padding

  // ---------- Firebase upload ----------
  if (millis() - lastUpload >= UPLOAD_INTERVAL) {
    lastUpload = millis();
    if (isOnline) publishToFirebase();
  }
}

// ---------------- Heat & Fan control helpers ----------------
void applyHeatLevel(int level) {
  switch (level) {
    case 0: Heat_off(); break;
    case 1: Heat_Level1(); break;
    case 2: Heat_Level2(); break;
    case 3: Heat_Level3(); break;
    default: Heat_off(); break;
  }
  heatLevel = level;
}

void Heat_off() {
  digitalWrite(relayPIN1, LOW);
  digitalWrite(relayPIN2, LOW);
  digitalWrite(relayPIN3, LOW);
}
void Heat_Level1() {
  digitalWrite(relayPIN1, HIGH);
  digitalWrite(relayPIN2, LOW);
  digitalWrite(relayPIN3, LOW);
}
void Heat_Level2() {
  digitalWrite(relayPIN1, HIGH);
  digitalWrite(relayPIN2, HIGH);
  digitalWrite(relayPIN3, LOW);
}
void Heat_Level3() {
  digitalWrite(relayPIN1, HIGH);
  digitalWrite(relayPIN2, HIGH);
  digitalWrite(relayPIN3, HIGH);
}
void Fan_on() {
  digitalWrite(relayPIN4, HIGH);
  fanState = true;
}
void Fan_off() {
  digitalWrite(relayPIN4, LOW);
  fanState = false;
}

// ---------------- Auto mode decision ----------------
void updateAutoMode() {
  Fan_on();
  if (currentTemp > 50.0) {
    applyHeatLevel(0);
  } else if (currentTemp > 40.0) {
    applyHeatLevel(1);
  } else if (currentTemp > 30.0) {
    applyHeatLevel(2);
  } else {
    applyHeatLevel(3);
  }
}

// ---------------- Firebase publish & read ----------------
void publishToFirebase() {
  if (!isOnline) return;
  time_t now = time(nullptr);
  String base = String(FB_BASE);

  Firebase.setFloat(fbdo, base + "temperature", currentTemp);
  Firebase.setFloat(fbdo, base + "humidity", currentHum);
  Firebase.setBool(fbdo, base + "fan_state", fanState);
  Firebase.setInt(fbdo, base + "heat_state", heatLevel);
  Firebase.setInt(fbdo, base + "timestamp", (int)now);
  Firebase.setString(fbdo, base + "mode", (currentMode == AUTO ? "Auto" : "Manual"));
}

void readCommandsFromFirebase() {
  if (!isOnline) return;
  String base = String(FB_BASE);

  if (Firebase.getString(fbdo, base + "mode")) {
    if (fbdo.stringData() == "Manual") currentMode = MANUAL;
    else currentMode = AUTO;
  }

  if (currentMode == MANUAL) {
    if (Firebase.getInt(fbdo, base + "heatLevel")) {
      int remoteHeat = fbdo.intData();
      if (remoteHeat >= 0 && remoteHeat <= 3 && remoteHeat != heatLevel) {
        heatLevel = remoteHeat;
      }
    }
    if (Firebase.getBool(fbdo, base + "fanState")) {
      bool remoteFan = fbdo.boolData();
      if (remoteFan != fanState) {
        fanState = remoteFan;
      }
    }
  }
}

// ---------------- Firebase & NTP setup ----------------
void setupFirebaseAndTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}
