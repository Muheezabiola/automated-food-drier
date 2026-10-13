#pragma once

#include <Arduino.h>

// ============================================================
//  PROJECT: Automated Food Drier
//  Single source of truth for pins, timings and thresholds.
//
//  Every module includes this header instead of hard-coding a pin
//  or a timing constant, so the wiring is described in exactly one
//  place. Credentials are deliberately NOT here: they live in
//  include/secrets.h, which is git-ignored.
// ============================================================

// --- Serial ---
#define SERIAL_BAUD 115200

// --- Credentials ---
// Real values live in include/secrets.h, which is git-ignored.
// Copy include/secrets.example.h to include/secrets.h first.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing include/secrets.h - copy include/secrets.example.h to include/secrets.h and fill in your credentials."
#endif

// ============================================================
//  I2C BUS AND DISPLAY
//  The 20x4 LCD is the only device on the bus, on the ESP32's
//  default I2C pins. Both are named here, so the wiring is stated
//  in one place rather than left implicit inside the driver.
// ============================================================
#define PIN_I2C_SDA 21
#define PIN_I2C_SCL 22

#define LCD_I2C_ADDRESS 0x27 // 0x3F is the other common backpack address
#define LCD_COLUMNS 20
#define LCD_ROWS 4

// ============================================================
//  AIR SENSOR  (DHT22 temperature and humidity)
// ============================================================
#define PIN_DHT 0
#define DHT_MODEL DHT22

// A DHT22 cannot answer faster than about once every two seconds, so
// the sensor module caches a reading and refreshes it at this
// interval instead of reading it on every pass.
#define DHT_REFRESH_MS 2000

// Decimal places the readings are shown with on the display.
#define SENSOR_READING_DECIMALS 1

// ============================================================
//  HEAT AND FAN OUTPUTS
//  Three relays switch the heating elements as a binary pattern and
//  a fourth switches the fan. All four are active HIGH.
// ============================================================
#define PIN_HEAT_1 16 // Heat bit 0 (least significant)
#define PIN_HEAT_2 17 // Heat bit 1
#define PIN_HEAT_3 18 // Heat bit 2
#define PIN_FAN 19    // Fan relay, switched independently of the pattern

#define HEAT_LEVEL_MIN 0
#define HEAT_LEVEL_MAX 3

// AUTO mode reads its heat level off the air temperature, hottest
// first: above the first threshold the elements stop, and below the
// last one all three run.
#define AUTO_HEAT_OFF_ABOVE_C 50.0f
#define AUTO_HEAT_LEVEL1_ABOVE_C 40.0f
#define AUTO_HEAT_LEVEL2_ABOVE_C 30.0f

// ============================================================
//  KEYPAD  (4x4 membrane keypad)
// ============================================================
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4
#define KEYPAD_ROW_PINS { 13, 12, 14, 27 }
#define KEYPAD_COL_PINS { 26, 25, 33, 32 }

// How long the last key stays echoed on the display.
#define KEY_DISPLAY_TIMEOUT_MS 3000UL

// ============================================================
//  CLOUD  (Firebase Realtime Database)
//  The node signs in as an email/password user. While it is offline
//  it retries that sign-in on the interval below rather than
//  blocking, so the drier keeps running on the keypad alone.
// ============================================================
#define FIREBASE_RETRY_MS 10000UL
#define UPLOAD_INTERVAL_MS 5000UL

#define NTP_SERVER_PRIMARY "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"

#define CLOUD_PATH_TEMPERATURE "/drier/temperature"
#define CLOUD_PATH_HUMIDITY "/drier/humidity"
#define CLOUD_PATH_FAN_STATE "/drier/fan_state"
#define CLOUD_PATH_HEAT_STATE "/drier/heat_state"
#define CLOUD_PATH_TIMESTAMP "/drier/timestamp"
#define CLOUD_PATH_MODE "/drier/mode"
#define CLOUD_PATH_HEAT_COMMAND "/drier/heatLevel" // Read back in MANUAL only
#define CLOUD_PATH_FAN_COMMAND "/drier/fanState"   // Read back in MANUAL only

#define CLOUD_MODE_AUTO "Auto"
#define CLOUD_MODE_MANUAL "Manual"

// ============================================================
//  BOOT TIMINGS
// ============================================================
#define BOOT_SETTLE_MS 50  // Pause after Serial begins, before the first line
#define SPLASH_HOLD_MS 500 // How long "Initializing..." stays on screen
