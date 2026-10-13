#include "drier.h"

#include <Arduino.h>

#include "cloud_link.h"
#include "config.h"
#include "display.h"
#include "heat_control.h"
#include "keypad_ui.h"
#include "sensors.h"

// ============================================================
//  Drier flow
//
//  Boot and one pass of loop(), in order. This file is the only
//  place that knows the order the modules run in; each of the others
//  answers for its own hardware and nothing else.
//
//  The order matters in one place: the keypad is scanned before the
//  database is read back, so a key press in MANUAL is what the pass
//  ends up acting on, and the publish slot runs last so the phone
//  sees the state the pass arrived at rather than the one it
//  started from.
// ============================================================

void drierInit()
{
    // The relay outputs go off before anything else, so an
    // interrupted boot cannot leave an element or the fan running.
    heatControlInit();

    Serial.begin(SERIAL_BAUD);
    delay(BOOT_SETTLE_MS);

    displayInit();
    sensorsInit();

    displayShowConnecting();
    cloudLinkInit();
}

void drierUpdate()
{
    cloudLinkUpdate();

    keypadScan();
    sensorsUpdate();

    // The database and the keypad drive the same state machine, so
    // the remote half is applied before the mode policy runs.
    keypadSyncFromCloud();
    keypadServiceMode(sensorsTemperatureC());

    displayRender(cloudLinkIsOnline(), keypadMode(), heatControlLevel(), heatControlFanOn(),
                  sensorsTemperatureC(), sensorsHumidityPct(), keypadLastKey(), keypadKeyEchoActive());

    if (cloudLinkDueForPublish())
    {
        cloudLinkPublishReadings(sensorsTemperatureC(), sensorsHumidityPct(), heatControlFanOn(),
                                 heatControlLevel(), keypadMode() == AUTO);
    }
}
