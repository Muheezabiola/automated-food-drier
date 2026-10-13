#include "sensors.h"

#include <DHT.h>

#include "config.h"

// ============================================================
//  DHT22 air sensor
//
//  The DHT22 is slow: it needs about two seconds between reads and
//  answers NaN while it is still busy. Rather than let either reach
//  the display or the database, the sensor is read on a timer and a
//  reading that did not arrive is dropped, leaving the last good
//  value in place.
//
//  Temperature and humidity come out of the same conversion, so they
//  are accepted or rejected together — a half-answer never becomes
//  half a reading.
// ============================================================

namespace
{
DHT g_dht(PIN_DHT, DHT_MODEL);

unsigned long g_lastReadMs = 0;
float g_temperatureC = 0.0f;
float g_humidityPct = 0.0f;
} // namespace

void sensorsInit()
{
    g_dht.begin();
}

void sensorsUpdate()
{
    if (millis() - g_lastReadMs < DHT_REFRESH_MS)
    {
        return;
    }

    g_lastReadMs = millis();

    const float temperatureC = g_dht.readTemperature();
    const float humidityPct = g_dht.readHumidity();

    if (isnan(temperatureC) || isnan(humidityPct))
    {
        return;
    }

    g_temperatureC = temperatureC;
    g_humidityPct = humidityPct;
}

float sensorsTemperatureC()
{
    return g_temperatureC;
}

float sensorsHumidityPct()
{
    return g_humidityPct;
}
