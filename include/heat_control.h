#pragma once

// The heat level, 0..3, and the fan relay.
//
// The level is a binary pattern over the three heat relays rather
// than three independent switches: level 1 lights the first element,
// level 2 the first two, level 3 all three. The fan is a fourth
// relay, wired outside that pattern, and is switched on its own.
//
// This module owns every relay write in the firmware — AUTO asks for
// a level derived from the air temperature, MANUAL holds the level
// the keypad or the database last set.
void heatControlInit();

int heatControlLevel();
bool heatControlFanOn();

// Writes the relays and remembers the level. A level outside 0..3
// switches all three elements off but is still stored, which is what
// the original sketch's applyHeatLevel() did.
void heatControlSetLevel(int heatLevel);
void heatControlSetFan(bool fanOn);

// AUTO policy: the fan runs, and the heat level falls as the air
// temperature rises past the AUTO_HEAT_* thresholds.
void heatControlApplyAuto(float temperatureC);
