#include "keypad_ui.h"

#include <Keypad.h>

#include "cloud_link.h"
#include "config.h"
#include "heat_control.h"

// ============================================================
//  Keypad and operating modes
//
//  The 4x4 keypad is the local half of the control loop and the
//  database is the remote half; both drive the state machine in
//  here, which is what keeps the phone and the keypad from fighting
//  over the heat level and the fan.
//
//  A key is acted on once, on the pass it is read, and whatever it
//  changed is published straight away. The remote heat level and fan
//  flag are ignored outside MANUAL: in AUTO the air temperature owns
//  both, and honouring a stale command there would leave the elements
//  on against the reading.
// ============================================================

namespace
{
char g_keyMap[KEYPAD_ROWS][KEYPAD_COLS] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

byte g_rowPins[KEYPAD_ROWS] = KEYPAD_ROW_PINS;
byte g_colPins[KEYPAD_COLS] = KEYPAD_COL_PINS;

Keypad g_keypad = Keypad(makeKeymap(g_keyMap), g_rowPins, g_colPins, KEYPAD_ROWS, KEYPAD_COLS);

Mode g_mode = AUTO; // The drier starts drying without a key press.
char g_lastKey = '\0';
unsigned long g_lastKeyMs = 0;
} // namespace

Mode keypadMode()
{
    return g_mode;
}

char keypadLastKey()
{
    return g_lastKey;
}

bool keypadKeyEchoActive()
{
    return g_lastKey != '\0' && (millis() - g_lastKeyMs) <= KEY_DISPLAY_TIMEOUT_MS;
}

void keypadScan()
{
    const char key = g_keypad.getKey();

    if (key == '\0')
    {
        return;
    }

    g_lastKey = key;
    g_lastKeyMs = millis();

    Serial.print("[keypad] key pressed: ");
    Serial.println(key);

    if (key == '#')
    {
        cloudLinkForceOffline();
    }
    else if (key == '*')
    {
        cloudLinkClearForceOffline();
    }
    else if (key == 'A')
    {
        g_mode = AUTO;
        heatControlSetFan(true);
        cloudLinkPublishMode(CLOUD_MODE_AUTO);
    }
    else if (key == 'B')
    {
        g_mode = MANUAL;
        cloudLinkPublishMode(CLOUD_MODE_MANUAL);
    }
    else if (g_mode == MANUAL)
    {
        if (key >= '0' && key <= '3')
        {
            const int heatLevel = key - '0';
            heatControlSetLevel(heatLevel);
            cloudLinkPublishHeatLevel(heatLevel);
        }
        else if (key == 'C')
        {
            heatControlSetFan(true);
            cloudLinkPublishFanState(true);
        }
        else if (key == 'D')
        {
            heatControlSetFan(false);
            cloudLinkPublishFanState(false);
        }
    }
}

void keypadSyncFromCloud()
{
    if (!cloudLinkIsOnline())
    {
        return;
    }

    String remoteMode;
    if (cloudLinkReadMode(remoteMode))
    {
        // Anything that is not the manual marker reads as AUTO, which
        // is how a cleared or unexpected value leaves the drier
        // drying rather than stopped.
        g_mode = (remoteMode == CLOUD_MODE_MANUAL) ? MANUAL : AUTO;
    }

    if (g_mode != MANUAL)
    {
        return;
    }

    int remoteHeatLevel = 0;
    if (cloudLinkReadHeatLevel(remoteHeatLevel))
    {
        const bool inRange = remoteHeatLevel >= HEAT_LEVEL_MIN && remoteHeatLevel <= HEAT_LEVEL_MAX;
        if (inRange && remoteHeatLevel != heatControlLevel())
        {
            heatControlSetLevel(remoteHeatLevel);
        }
    }

    bool remoteFanOn = false;
    if (cloudLinkReadFanState(remoteFanOn) && remoteFanOn != heatControlFanOn())
    {
        heatControlSetFan(remoteFanOn);
    }
}

void keypadServiceMode(float temperatureC)
{
    if (g_mode == AUTO)
    {
        heatControlApplyAuto(temperatureC);
        return;
    }

    // MANUAL: the stored level and fan flag are the whole truth, and
    // they are re-applied here so a command that only changed the
    // stored value takes effect on the pass that read it.
    heatControlSetLevel(heatControlLevel());
    heatControlSetFan(heatControlFanOn());
}
