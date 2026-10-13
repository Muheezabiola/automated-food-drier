#pragma once

#include <Arduino.h>

// The 4x4 keypad and the operating modes it selects.
//
// The keypad is the local half of the control loop and the database
// is the remote half; both drive the state machine in here, which is
// what stops the two of them fighting over the heat level and the
// fan.
//
// MODE_IDLE is the state the machine starts in before either mode is
// chosen. The drier boots straight into AUTO, so nothing selects it
// in normal use.
enum Mode
{
    MODE_IDLE,
    AUTO,
    MANUAL
};

// Reads one key and acts on it. A key that changes something is
// published immediately, so the database follows the keypad without
// waiting for the next upload slot.
void keypadScan();

Mode keypadMode();

// The last key seen, and whether it is still inside the echo window
// the display shows it for.
char keypadLastKey();
bool keypadKeyEchoActive();

// Applies the mode the database reports and, in MANUAL, the heat
// level and fan flag it carries. Those two are deliberately ignored
// in AUTO, where the air temperature decides both.
void keypadSyncFromCloud();

// The mode policy: AUTO hands the fan and the heat to the air
// temperature, MANUAL drives both from the stored level and flag.
void keypadServiceMode(float temperatureC);
