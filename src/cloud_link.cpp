#include "cloud_link.h"

#include <FirebaseESP32.h>
#include <WiFi.h>

#include <time.h>

#include "config.h"

// ============================================================
//  Cloud link: Wi-Fi, NTP and the Firebase Realtime Database
//
//  The Firebase ESP32 Client — the v4 library with FirebaseData,
//  FirebaseAuth and FirebaseConfig — is confined to this file. The
//  rest of the firmware talks to the database only through the
//  cloudLink* calls in cloud_link.h, so replacing the client would
//  not reach any other module.
//
//  Sign-in is retried on a timer instead of blocking the boot, so a
//  network that is not up yet costs the drier nothing but an OFFLINE
//  display. Each retry starts the clock as well: the timestamp the
//  node publishes is only as good as the last NTP sync behind it.
//
//  The forced-offline flag is what the '#' and '*' keys set and
//  clear. It is deliberately not a disconnect: the Wi-Fi association
//  is left alone and the node simply stops using the database.
// ============================================================

namespace
{
FirebaseData g_fbdo;
FirebaseAuth g_auth;
FirebaseConfig g_config;

bool g_online = false;
bool g_forceOffline = false;
unsigned long g_lastAttemptMs = 0;
unsigned long g_lastUploadMs = 0;

// Signs in and starts the clock. Called again on every retry, which
// is how the client recovers from a network that was not there at
// boot.
void beginFirebaseAndTime()
{
    configTime(0, 0, NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY);

    g_auth.user.email = FIREBASE_USER_EMAIL;
    g_auth.user.password = FIREBASE_USER_PASSWORD;
    g_config.api_key = FIREBASE_API_KEY;
    g_config.database_url = FIREBASE_DATABASE_URL;

    Firebase.begin(&g_config, &g_auth);
    Firebase.reconnectNetwork(true);
}
} // namespace

void cloudLinkInit()
{
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("[cloud] Wi-Fi connection attempt started");

    // Both slots start now, so the first retry and the first upload
    // each land one full interval after boot.
    g_lastAttemptMs = millis();
    g_lastUploadMs = millis();
}

void cloudLinkUpdate()
{
    const bool wifiUp = (WiFi.status() == WL_CONNECTED);

    if (wifiUp && !g_online && !g_forceOffline)
    {
        if (!Firebase.ready() && millis() - g_lastAttemptMs > FIREBASE_RETRY_MS)
        {
            g_lastAttemptMs = millis();
            beginFirebaseAndTime();
        }

        if (Firebase.ready())
        {
            g_online = true;
            Serial.println("[cloud] Firebase is ready");
        }
    }
    else if ((!wifiUp || g_forceOffline) && g_online)
    {
        g_online = false;
        Serial.println("[cloud] offline");
    }
}

bool cloudLinkIsOnline()
{
    return g_online;
}

void cloudLinkForceOffline()
{
    g_forceOffline = true;
}

void cloudLinkClearForceOffline()
{
    g_forceOffline = false;
}

bool cloudLinkDueForPublish()
{
    if (millis() - g_lastUploadMs < UPLOAD_INTERVAL_MS)
    {
        return false;
    }

    g_lastUploadMs = millis();
    return true;
}

void cloudLinkPublishReadings(float temperatureC, float humidityPct, bool fanOn, int heatLevel, bool autoMode)
{
    if (!g_online)
    {
        return;
    }

    const int timestamp = static_cast<int>(time(nullptr));

    Firebase.setFloat(g_fbdo, CLOUD_PATH_TEMPERATURE, temperatureC);
    Firebase.setFloat(g_fbdo, CLOUD_PATH_HUMIDITY, humidityPct);
    Firebase.setBool(g_fbdo, CLOUD_PATH_FAN_STATE, fanOn);
    Firebase.setInt(g_fbdo, CLOUD_PATH_HEAT_STATE, heatLevel);
    Firebase.setInt(g_fbdo, CLOUD_PATH_TIMESTAMP, timestamp);
    Firebase.setString(g_fbdo, CLOUD_PATH_MODE, String(autoMode ? CLOUD_MODE_AUTO : CLOUD_MODE_MANUAL));
}

void cloudLinkPublishMode(const char* modeName)
{
    if (!g_online)
    {
        return;
    }

    Firebase.setString(g_fbdo, CLOUD_PATH_MODE, String(modeName));
}

void cloudLinkPublishHeatLevel(int heatLevel)
{
    if (!g_online)
    {
        return;
    }

    Firebase.setInt(g_fbdo, CLOUD_PATH_HEAT_COMMAND, heatLevel);
}

void cloudLinkPublishFanState(bool fanOn)
{
    if (!g_online)
    {
        return;
    }

    Firebase.setBool(g_fbdo, CLOUD_PATH_FAN_COMMAND, fanOn);
}

bool cloudLinkReadMode(String& modeOut)
{
    if (!g_online || !Firebase.getString(g_fbdo, CLOUD_PATH_MODE))
    {
        return false;
    }

    modeOut = g_fbdo.stringData();
    return true;
}

bool cloudLinkReadHeatLevel(int& heatLevelOut)
{
    if (!g_online || !Firebase.getInt(g_fbdo, CLOUD_PATH_HEAT_COMMAND))
    {
        return false;
    }

    heatLevelOut = g_fbdo.intData();
    return true;
}

bool cloudLinkReadFanState(bool& fanOnOut)
{
    if (!g_online || !Firebase.getBool(g_fbdo, CLOUD_PATH_FAN_COMMAND))
    {
        return false;
    }

    fanOnOut = g_fbdo.boolData();
    return true;
}
