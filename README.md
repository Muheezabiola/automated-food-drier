# Automated Food Drier

Firmware for a food drier built on an ESP32: three heating elements, a fan, a 20×4 display and a 4×4 keypad, with the whole run mirrored into a Firebase Realtime Database.

It does two things at once. It **runs the drier**: three relays switch the heating elements as a binary pattern, a fourth switches the fan, and the heat level is either derived from the air temperature or set by hand. And it **reports and takes orders**: air temperature, humidity, fan state, heat level, timestamp and mode go up to the database every five seconds, and the mode, the heat level and the fan flag come back down — the same state machine is reachable from the keypad on the front and from anywhere a phone can reach the database.

The firmware is a PlatformIO project built from modules, one per concern, with a single environment for the one board it runs on.

| Board | Environment | Framework |
| --- | --- | --- |
| ESP32 DevKit (ESP32-WROOM-32) | `esp32dev` | Arduino |

---

## Table of Contents

- [Features](#features)
- [How It Works](#how-it-works)
- [Operating Modes](#operating-modes)
- [Keypad Reference](#keypad-reference)
- [Architecture](#architecture)
- [Project Structure](#project-structure)
- [Hardware Requirements](#hardware-requirements)
- [Wiring and Pin Map](#wiring-and-pin-map)
- [Getting Started](#getting-started)
- [Credentials and Secrets](#credentials-and-secrets)
- [Cloud Data Model](#cloud-data-model)
- [Configuration Reference](#configuration-reference)
- [Troubleshooting](#troubleshooting)
- [Design Notes and Limitations](#design-notes-and-limitations)
- [License](#license)

---

## Features

- **One heat setting over three relays** — a level from 0 to 3 drives the elements as a binary pattern (1 = first element, 2 = first two, 3 = all three), so the heat setting is one number on the display, in the database and on the keypad instead of three switches to keep in step.
- **AUTO that follows the air** — the fan runs and the heat level falls as the DHT22 reading climbs, with nothing to set and nothing to drift.
- **MANUAL that ignores the sensor** — the heat level and the fan flag come from the keypad or from the database, and the temperature stops deciding anything.
- **Two controls, one state machine** — a key press is published immediately and the database is read back on every pass, so the keypad and the phone drive the same state rather than two copies of it.
- **A cloud link that never blocks** — the Wi-Fi association and the Firebase sign-in are retried on a timer, so a drier with no network still heats, still displays and still takes orders from the keypad.
- **Forced offline on demand** — `#` stops the node talking to the database, `*` lets it try again. The Wi-Fi link is left alone.
- **A display that does not flicker** — the four rows are repainted on every pass behind one padding rule, so a shorter line cannot leave the tail of the previous screen standing behind it, and the display never has to be cleared.
- **Fail-fast configuration** — the build stops with a clear `#error` if the secrets file is missing.
- **Secrets kept out of Git** — every credential lives in `include/secrets.h`, which is git-ignored, with a committed `include/secrets.example.h` template.
- **One module per concern** — pins and timings in one header, a `.cpp` per module, and a `main.cpp` that does nothing but hand over to the drier flow.

---

## How It Works

### Boot

1. Configure the relay outputs with the elements and the fan off. That happens before anything else, so an interrupted boot cannot leave the drier heating.
2. Start the serial log at 115200 baud.
3. Start the display, show `Initializing...` and hold it for half a second.
4. Start the DHT22.
5. Show the `FOOD DRIER` title with `Connecting WiFi...` underneath, then start the Wi-Fi association and stamp the retry and upload timers.

The boot does **not** wait for the network. The association and the Firebase sign-in are retried from `loop()`, so a board with no network shows `OFFLINE` on row 1 and keeps drying on its own.

### One pass of `loop()`

1. **Cloud link** — if Wi-Fi is up, the node is not forced offline and the client is not signed in yet, retry the sign-in every 10 s; the moment the client reports ready, go online. If Wi-Fi drops or the node is forced offline, go offline.
2. **Keypad** — read one key and act on it, publishing whatever it changed.
3. **Air sensor** — read the DHT22 only if two seconds have passed since the last read; a reading that came back `NaN` is dropped and the last good pair stays.
4. **Remote half** — while online, read the mode back from the database and apply it; in MANUAL, read the heat level and the fan flag back as well.
5. **Mode policy** — AUTO hands the fan and the heat level to the air temperature; MANUAL re-applies the level and fan flag it is holding.
6. **Display** — the four rows, redrawn from the state the modules now hold.
7. **Upload** — if the five-second slot has come round, publish the whole record.

The upload slot in step 7 is spent whether or not the node is online, which is what keeps the published timestamps five seconds apart rather than piling up behind a long outage.

### Timing

```
        every pass              every 2 s            every 5 s            every 10 s
   ┌────────────────────┐  ┌──────────────────┐  ┌────────────────┐  ┌────────────────┐
   │ display redraw     │  │ DHT22 reading    │  │ Firebase write │  │ sign-in retry  │
   │ keypad scan        │  │ (cached between) │  │ of the record  │  │ while offline  │
   │ mode policy        │  └──────────────────┘  └────────────────┘  └────────────────┘
   │ command read-back  │
   └────────────────────┘
```

---

## Operating Modes

| Mode | Selected by | Heat | Fan | Left by |
| --- | --- | --- | --- | --- |
| `AUTO` | `A` on the keypad, or `mode` in the database reading anything other than `Manual` | air temperature: above 50 °C off, above 40 °C level 1, above 30 °C level 2, otherwise level 3 | always on | `B`, or the database |
| `MANUAL` | `B` on the keypad, or `mode` = `Manual` in the database | the level held in the node, set by `0`..`3` or by `heatLevel` | the fan flag held in the node, set by `C`/`D` or by `fanState` | `A`, or the database |
| Forced offline | `#` on the keypad; cleared by `*` | whatever mode is already running | unchanged | `*` on the keypad |

The drier boots into `AUTO` without a key press, which is what the original sketch did.

Forced offline is not a mode of the heat, it is a property of the link: the node keeps heating, keeps displaying and keeps taking key presses, and simply stops reading and writing the database. It is also what makes the keypad authoritative again, because a mode set on the keypad can no longer be overwritten by `mode` coming back down.

`MODE_IDLE` exists in the state machine but nothing selects it: it is the state the machine would start in before either mode is chosen.

---

## Keypad Reference

| Key | Applied in | Action | Published to the database |
| --- | --- | --- | --- |
| `A` | any mode | Switch to AUTO, and turn the fan on | `mode` = `Auto` |
| `B` | any mode | Switch to MANUAL; heat and fan keep the values they hold | `mode` = `Manual` |
| `0` | MANUAL | Heat level 0 — all three elements off | `heatLevel` = 0 |
| `1` | MANUAL | Heat level 1 — first element on | `heatLevel` = 1 |
| `2` | MANUAL | Heat level 2 — first two elements on | `heatLevel` = 2 |
| `3` | MANUAL | Heat level 3 — all three elements on | `heatLevel` = 3 |
| `C` | MANUAL | Fan on | `fanState` = true |
| `D` | MANUAL | Fan off | `fanState` = false |
| `#` | any mode | Force offline — stop using the database | — |
| `*` | any mode | Clear forced offline — let the sign-in retry run | — |
| any other key | any mode | Nothing, and no write | — |

The last key pressed is echoed on row 3 for three seconds, then that field goes back to `-`.

---

## Architecture

```
 ┌──────────────────────────────────────────────────────────────┐
 │                    Automated Food Drier                      │
 │                                                              │
 │  sensors ──────┐                                             │
 │                │                                             │
 │  keypad_ui ────┼──▶ heat_control ──▶ heat relays + fan relay │
 │      ▲         │         ▲                                   │
 │      │         │         │                                   │
 │      │         └──▶ display (4 rows, one padding rule)        │
 │      │                                                       │
 │      └─◀── cloud_link ◀──▶ Firebase Realtime Database         │
 └──────────────────────────────────────────────────────────────┘
```

`drier` owns the order of the arrows; `main.cpp` owns nothing at all.

The one loop in that diagram worth naming is `keypad_ui` ← `cloud_link`: the commands the database carries are applied by the same module that reads the keypad, so the two controls converge before the heat or the display ever see them.

Two shared services sit on the outside: `display` (every row goes through one padding rule, so a row is always 20 characters and nothing from the previous screen shows through) and `heat_control` (every relay write, so the elements are never driven from two places at once).

---

## Project Structure

```
automated-food-drier/
├── platformio.ini              One environment: esp32dev
├── README.md
├── LICENSE
├── .gitignore                  Build output, editor state, include/secrets.h
├── include/
│   ├── config.h                Pins, timings and thresholds
│   ├── secrets.example.h       Credentials template (committed)
│   ├── secrets.h               Credentials (git-ignored, yours to create)
│   ├── drier.h                 Top-level flow: boot and one pass of loop()
│   ├── cloud_link.h            Wi-Fi, NTP and the Firebase Realtime Database
│   ├── sensors.h               DHT22 air temperature and humidity
│   ├── heat_control.h          Heat level 0..3 and the fan relay
│   ├── keypad_ui.h             Keypad scan, operating modes and key actions
│   └── display.h               The 20x4 display and its padding rule
├── src/                        One .cpp per header, plus main.cpp
└── legacy/                     The original prototype sketch (not compiled)
```

`legacy/food_drier.ino` is the single sketch this project was ported from: one file, everything in it, credentials included. It is kept for reference. PlatformIO compiles `src/` only, so it sits there without affecting the build — and the credentials in it have been replaced with placeholders, because a working Wi-Fi password has no business in a file that gets committed.

---

## Hardware Requirements

| Part | Quantity | Notes |
| --- | --- | --- |
| ESP32 DevKit (ESP32-WROOM-32) | 1 | The controller |
| 20×4 I2C LCD (PCF8574 backpack) | 1 | Address `0x27` |
| DHT22 temperature/humidity sensor | 1 | Adafruit's DHT driver also covers the DHT11 |
| 10 kΩ resistor | 1 | Pull-up on the DHT22 data line |
| 4-channel relay module | 1 | Three elements plus the fan; opto-isolated modules are worth the extra cost |
| 4×4 membrane keypad | 1 | Rows and columns direct to GPIO, no driver needed |
| 5 V supply, ≥ 1 A | 1 | Not a weak USB port: four relay coils and the LCD backlight all draw from it |
| Jumper wires | — | Two for I2C, plus one per element, plus four rows and four columns |

The heating elements themselves are whatever the drier was built around; the firmware only knows about the relays that switch them.

---

## Wiring and Pin Map

| Signal | GPIO | Direction | Notes |
| --- | --- | --- | --- |
| DHT22 data | 0 | in/out | Needs the 10 kΩ pull-up to 3.3 V |
| Heat relay 1 (bit 0) | 16 | out | HIGH energises the relay |
| Heat relay 2 (bit 1) | 17 | out | HIGH energises the relay |
| Heat relay 3 (bit 2) | 18 | out | HIGH energises the relay |
| Fan relay | 19 | out | Outside the heat pattern |
| LCD SDA / SCL | 21 / 22 | out | I2C, address `0x27`, the ESP32 defaults made explicit |
| Keypad rows 1–4 | 13, 12, 14, 27 | out/in | Column scan, pulled low by a press |
| Keypad columns 1–4 | 26, 25, 33, 32 | in | Column scan, read by the driver |

The heater relays are wired to the elements as a binary pattern, so the level in the database maps onto the hardware like this:

| Level | Relay 1 (16) | Relay 2 (17) | Relay 3 (18) | Fan (19) |
| --- | --- | --- | --- | --- |
| 0 | off | off | off | on in AUTO, flag in MANUAL |
| 1 | on | off | off | on in AUTO, flag in MANUAL |
| 2 | on | on | off | on in AUTO, flag in MANUAL |
| 3 | on | on | on | on in AUTO, flag in MANUAL |

GPIO0 is a strapping pin, which is worth knowing because it is also the DHT22 data line: the sensor must not hold it low while the board is powering up, or the board comes up in the serial bootloader instead of running the firmware. The 10 kΩ pull-up to 3.3 V is what keeps it high; check it before blaming the sensor.

---

## Getting Started

### 1. Prerequisites

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide), or PlatformIO Core 6.x on the CLI
- `platform = espressif32 @ ^7.0.1`, `framework = arduino` (pinned in `platformio.ini`, because the Arduino core changes between platform majors)

Dependencies are declared in `platformio.ini` and installed automatically on the first build:

| Library | Used for |
| --- | --- |
| `mobizt/Firebase ESP32 Client` | Realtime Database over TLS, with email/password auth (the v4 `FirebaseData`/`FirebaseAuth`/`FirebaseConfig` API) |
| `marcoschwartz/LiquidCrystal_I2C` | The 20×4 display |
| `chris--a/Keypad` | The 4×4 matrix keypad |
| `adafruit/DHT sensor library` | The DHT22 (pulls in Adafruit Unified Sensor) |

### 2. Create your credentials file

Nothing compiles until this exists — the build deliberately stops with:

```
#error "Missing include/secrets.h - copy include/secrets.example.h to include/secrets.h ..."
```

```bash
copy include\secrets.example.h include\secrets.h     # Windows
cp include/secrets.example.h include/secrets.h       # macOS / Linux
```

Then edit `include/secrets.h` — see [Credentials and Secrets](#credentials-and-secrets).

### 3. Build and upload

Pick the environment in the PlatformIO toolbar and press **Upload**, or use the CLI:

```bash
pio run -e esp32dev -t upload
```

Or build without uploading:

```bash
pio run -e esp32dev
```

### 4. Run

Open the serial monitor at 115200 baud:

```
[cloud] Wi-Fi connection attempt started
[cloud] Firebase is ready
[keypad] key pressed: A
[cloud] offline
```

The four display rows in normal operation:

```
    FOOD DRIER
 ONLINE  | AUTO
Heat:3  Fan:ON
T:24.5C H:58.2% K:A
```

Row 1 is the link and the mode, row 2 is what the relays are doing, row 3 is the air and the last key pressed.

---

## Credentials and Secrets

Every credential lives in `include/secrets.h`, which **is listed in [.gitignore](./.gitignore) and must never be committed**. The file is seeded with the values that were hard-coded in the original sketch, so the drier keeps talking to the same network and the same database it always did — but they now stay on the machine that builds the firmware.

| Key | Used for | Where to get it |
| --- | --- | --- |
| `WIFI_SSID` / `WIFI_PASSWORD` | the network the drier joins (2.4 GHz) | your router |
| `FIREBASE_API_KEY` | Firebase project web API key | Firebase console → Project settings |
| `FIREBASE_USER_EMAIL` / `FIREBASE_USER_PASSWORD` | the drier's own database user | Firebase console → Authentication |
| `FIREBASE_DATABASE_URL` | the Realtime Database instance | Firebase console → Realtime Database |

> **Rotate all of these.** They were published in plain source in the original sketch, so anyone who has read it has seen the Wi-Fi password, the Firebase user's password and the database URL. Moving them out of the source does not un-publish them; replacing them does.

> **Lock the Firebase user down.** The node signs in as a user and touches a fixed set of `/drier` paths. The database rules should restrict that account to those paths and nothing else, so a leaked `secrets.h` cannot be used to rewrite the whole project. The web API key on its own is not a secret — the account behind it is.

Before pushing the project to GitHub:

```bash
git check-ignore -v include/secrets.h    # must print the .gitignore match
git status --porcelain                   # secrets.h must not appear
```

---

## Cloud Data Model

The node writes these paths under `/drier/` in the Realtime Database, and reads some of them back:

| Path | Type | Written | Meaning |
| --- | --- | --- | --- |
| `/drier/temperature` | number | every 5 s | Air temperature in °C |
| `/drier/humidity` | number | every 5 s | Relative humidity in % |
| `/drier/fan_state` | boolean | every 5 s | What the fan relay is actually doing |
| `/drier/heat_state` | number | every 5 s | What the heat relays are actually doing, 0..3 |
| `/drier/timestamp` | number | every 5 s | The node's clock, in seconds since the epoch |
| `/drier/mode` | string | every 5 s, and on `A`/`B` | `Auto` or `Manual` |
| `/drier/heatLevel` | number | on `0`..`3` | The level MANUAL should hold, 0..3 |
| `/drier/fanState` | boolean | on `C`/`D` | The fan flag MANUAL should hold |

Three of those are read back as well:

| Path | Read | Applied |
| --- | --- | --- |
| `/drier/mode` | every pass, while online | Anything other than `Manual` is AUTO |
| `/drier/heatLevel` | every pass, while online, **MANUAL only** | Stored if it is 0..3 and differs from the level already held |
| `/drier/fanState` | every pass, while online, **MANUAL only** | Stored if it differs from the flag already held |

Note the two spellings: `heat_state` and `fan_state` (underscores) are what the node reports, while `heatLevel` and `fanState` (camel case) are the commands it reads back. The asymmetry is carried over from the original sketch, and it is the one thing to get right when writing a remote.

A DHT22 reading that did not arrive is not published at all, so the database keeps the last good value instead of losing it to a `NaN`.

---

## Configuration Reference

Everything tunable lives in [include/config.h](./include/config.h).

### Serial and I2C

| Constant | Default | Meaning |
| --- | --- | --- |
| `SERIAL_BAUD` | `115200` | Serial log speed |
| `PIN_I2C_SDA` / `PIN_I2C_SCL` | 21 / 22 | The ESP32 defaults, made explicit |
| `LCD_I2C_ADDRESS` | `0x27` | Common backpack address; `0x3F` is the other |
| `LCD_COLUMNS` / `LCD_ROWS` | 20 / 4 | Also the width a row is padded and clipped to |

### Air sensor

| Constant | Default | Meaning |
| --- | --- | --- |
| `PIN_DHT` / `DHT_MODEL` | 0 / `DHT22` | The sensor and the driver to use for it |
| `DHT_REFRESH_MS` | `2000` | How often the DHT22 is actually read |
| `SENSOR_READING_DECIMALS` | 1 | Decimal places on the display |

### Heat and fan outputs

| Constant | Default | Meaning |
| --- | --- | --- |
| `PIN_HEAT_1` / `PIN_HEAT_2` / `PIN_HEAT_3` | 16 / 17 / 18 | Heat bits 0, 1 and 2, all active HIGH |
| `PIN_FAN` | 19 | The fan relay, outside the heat pattern |
| `HEAT_LEVEL_MIN` / `HEAT_LEVEL_MAX` | 0 / 3 | Range the level is allowed to take |
| `AUTO_HEAT_OFF_ABOVE_C` | 50.0 | Above this, AUTO switches every element off |
| `AUTO_HEAT_LEVEL1_ABOVE_C` | 40.0 | Above this, AUTO holds level 1 |
| `AUTO_HEAT_LEVEL2_ABOVE_C` | 30.0 | Above this, AUTO holds level 2; below it, level 3 |

### Keypad

| Constant | Default | Meaning |
| --- | --- | --- |
| `KEYPAD_ROWS` / `KEYPAD_COLS` | 4 / 4 | Matrix size |
| `KEYPAD_ROW_PINS` | `{ 13, 12, 14, 27 }` | Rows 1–4 |
| `KEYPAD_COL_PINS` | `{ 26, 25, 33, 32 }` | Columns 1–4 |
| `KEY_DISPLAY_TIMEOUT_MS` | `3000` | How long the last key stays on row 3 |

### Cloud

| Constant | Default | Meaning |
| --- | --- | --- |
| `FIREBASE_RETRY_MS` | `10000` | Gap between sign-in attempts while offline |
| `UPLOAD_INTERVAL_MS` | `5000` | Publish interval |
| `NTP_SERVER_PRIMARY` / `_SECONDARY` | `pool.ntp.org` / `time.nist.gov` | Clock sources, set on every sign-in attempt |
| `CLOUD_PATH_TEMPERATURE` | `/drier/temperature` | Published reading |
| `CLOUD_PATH_HUMIDITY` | `/drier/humidity` | Published reading |
| `CLOUD_PATH_FAN_STATE` | `/drier/fan_state` | Published output state |
| `CLOUD_PATH_HEAT_STATE` | `/drier/heat_state` | Published output state |
| `CLOUD_PATH_TIMESTAMP` | `/drier/timestamp` | Published clock |
| `CLOUD_PATH_MODE` | `/drier/mode` | Published and read back |
| `CLOUD_PATH_HEAT_COMMAND` | `/drier/heatLevel` | Written by the keypad, read back in MANUAL |
| `CLOUD_PATH_FAN_COMMAND` | `/drier/fanState` | Written by the keypad, read back in MANUAL |
| `CLOUD_MODE_AUTO` / `CLOUD_MODE_MANUAL` | `Auto` / `Manual` | The two values `/drier/mode` carries |

### Boot timings

| Constant | Default | Meaning |
| --- | --- | --- |
| `BOOT_SETTLE_MS` | `50` | Pause after `Serial.begin()` before the first line |
| `SPLASH_HOLD_MS` | `500` | How long `Initializing...` stays on screen |

---

## Troubleshooting

| Symptom | Likely cause / fix |
| --- | --- |
| `Missing include/secrets.h` during build | Copy `include/secrets.example.h` to `include/secrets.h`. This is intentional |
| Row 1 stays `OFFLINE` and the log only ever shows the Wi-Fi attempt | Wrong SSID/password in `secrets.h`, or a 5 GHz-only network — the ESP32 is 2.4 GHz only |
| Wi-Fi connects but the log never says `Firebase is ready` | Wrong API key, wrong user, wrong database URL, or no internet. The sign-in is retried every 10 s, so watch the log rather than the display |
| Nothing is written to the database although the node is online | The database rules are refusing the writes: check the rules against the paths in [Cloud Data Model](#cloud-data-model) |
| The keypad works but a key seems to be undone | While online, `/drier/mode`, `heatLevel` and `fanState` are read back on every pass and win over the keypad. Press `#` for local-only control, or fix the write that is failing |
| The node will not talk to the database at all, on purpose | `#` was pressed. `*` clears it; the link then needs to be ready again, which takes up to 10 s |
| AUTO never drops below level 3 | The readings are below 30 °C. That is the ladder doing what it is told — see the [design notes](#design-notes-and-limitations) |
| Heat level never changes in AUTO | The temperature is above 50 °C, where AUTO switches every element off |
| Temperature sits at `0.0` | No reading has ever arrived: check the DHT22 wiring, the 10 kΩ pull-up and the 3.3 V supply. A `NaN` reading is dropped, so the last good value (0.0 until the first one) stays on screen |
| Board does not boot, or the log is silent | GPIO0 is the DHT22 data line *and* a strapping pin: it must not be held low while the board powers up. Check the pull-up |
| LCD is blank, or shows blocks | I2C address (`0x27` vs `0x3F`), SDA/SCL swapped, or the contrast potentiometer on the backpack |
| The keypad reports the wrong characters | The row and column pin lists are in the wrong order — a swapped row pair shifts every key in that row |
| Node browns out, or the relays chatter | Four relay coils plus the LCD backlight want more than a weak USB port supplies; use a 5 V ≥ 1 A supply |
| The published `timestamp` is a small number | The clock has not synced yet: `time()` returns 0 until the first NTP exchange succeeds, which happens on a sign-in attempt |

---

## Design Notes and Limitations

- **AUTO is a level ladder, not a thermostat.** Above 50 °C every element is off, above 40 °C one is on, above 30 °C two, and below 30 °C all three. There is no setpoint, no hysteresis and no off delay between steps, so a reading sitting exactly on a threshold can switch the relays as often as the DHT22 refreshes. The ladder is carried over from the original sketch.
- **Nothing measures heat, only air.** One DHT22 in the chamber is the entire sensor set. There is no element thermostat, no food-moisture sensing and no airflow measurement, so the level is an open-loop count of elements rather than a temperature the drier can hit.
- **The database wins while the node is online.** `/drier/mode` is read back on every pass and applied, so a key press whose write does not land is undone on the next pass. The keypad is only unquestioned while the node is offline or forced offline.
- **Forced offline is not persisted, and only shows as `OFFLINE`.** A reboot forgets it, and row 1 cannot tell "no Wi-Fi" apart from "asked not to talk". The serial log can.
- **A failed write is not retried.** A reading that did not reach the database is not queued; the next publish slot five seconds later carries the next one. There is no store-and-forward.
- **The published timestamp depends on NTP.** `configTime()` runs on every sign-in attempt, and `time()` returns 0 until an exchange succeeds, so an early record can carry a timestamp of 0. Nothing waits for the clock.
- **The TLS chain is the library's default.** No root CA is pinned in this project, so verification rests on the certificate bundle the ESP32 core ships with. A network that inspects or replaces certificates will not get a sign-in, and the node stays `OFFLINE`.
- **The Firebase user's password is stored in plaintext on the device.** That is how the email/password flow works; the database rules are what limit the damage if the board is taken.
- **The display is clipped, not scrolled.** A row is written as exactly 20 characters, so an overlong value is cut off rather than allowed to run onto the row below. A temperature with a sign and two digits is the widest it gets in practice.
- **Nothing is persisted locally.** A power cut loses the mode and the last key: the drier comes back up in AUTO with the elements off, which is the safe direction to fail in.
- **GPIO12 is a strapping pin, and it sits on the keypad.** MTDI is sampled at power-up to pick the flash voltage, so a board that sees it high there can come up on 1.8 V flash and fail to boot. It is a *row* pin here, and the Keypad library holds its rows on `INPUT_PULLUP` between scans — a pairing carried over from the original sketch. If the board is unreliable about booting, moving that row to a free pin is the first thing to try.
- **The keypad echo is the only user feedback.** There is no buzzer and no status LED in this project, so a rejected key, a blocked write and a dead network all look the same on the display: nothing happens.

---

## License

Released under the MIT License — see [LICENSE](./LICENSE).
