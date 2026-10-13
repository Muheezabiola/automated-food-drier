#pragma once

#include "keypad_ui.h"

// The 20x4 character display on its I2C backpack.
//
// Every row is written as exactly LCD_COLUMNS characters, so a
// shorter line cannot leave the tail of the previous screen standing
// behind it — which is what lets the display be repainted on every
// pass without clearing it and flickering.
void displayInit();

// The boot screen: the title, and the Wi-Fi notice that stays up
// while the association is attempted.
void displayShowConnecting();

// The four rows, redrawn from the state the other modules hold.
void displayRender(bool online, Mode mode, int heatLevel, bool fanOn, float temperatureC, float humidityPct, char lastKey, bool keyEcho);
