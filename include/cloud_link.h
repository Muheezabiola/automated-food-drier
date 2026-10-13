#pragma once

#include <Arduino.h>

// Cloud link: Wi-Fi, NTP and the Firebase Realtime Database, signed
// in as an email/password user.
//
// The Firebase client is wrapped here and nowhere else, so no other
// module sees FirebaseData, FirebaseAuth or FirebaseConfig.
//
// cloudLinkUpdate() retries the sign-in every FIREBASE_RETRY_MS while
// the node is offline; until it succeeds every read and write below
// is a no-op and the drier runs on the keypad alone. Nothing here
// blocks: a boot without a network still reaches the end of setup().
void cloudLinkInit();

// One pass of the retry and ready logic. Also the only place the
// online flag is set, so the display can be trusted to tell the
// truth about the link.
void cloudLinkUpdate();

// True while the database link is up. This is what the display means
// by ONLINE, and what gates every read and write in this file.
bool cloudLinkIsOnline();

// '#' drops the link on purpose, '*' lets the retry run again. The
// Wi-Fi association is left alone; the node simply stops talking to
// the database, which is what "forced offline" means here.
void cloudLinkForceOffline();
void cloudLinkClearForceOffline();

// True once UPLOAD_INTERVAL_MS has passed. The slot is spent either
// way, online or not — the original sketch stamped its upload timer
// unconditionally, and the interval that the phone sees depends on
// that.
bool cloudLinkDueForPublish();

// Publishes the readings, the outputs, the clock and the mode, which
// is the whole record the phone reads back.
void cloudLinkPublishReadings(float temperatureC, float humidityPct, bool fanOn, int heatLevel, bool autoMode);

// Written the moment a key changes something, so the phone and the
// keypad cannot disagree for longer than one publish interval.
void cloudLinkPublishMode(const char* modeName);
void cloudLinkPublishHeatLevel(int heatLevel);
void cloudLinkPublishFanState(bool fanOn);

// Read back from the database. Each returns false when the read or
// the conversion failed, in which case the out parameter is
// untouched and the caller keeps the state it had.
bool cloudLinkReadMode(String& modeOut);
bool cloudLinkReadHeatLevel(int& heatLevelOut);
bool cloudLinkReadFanState(bool& fanOnOut);
