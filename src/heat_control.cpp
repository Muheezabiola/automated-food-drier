#include "heat_control.h"

#include "config.h"

// ============================================================
//  Heat level and fan relay
//
//  The three heat relays are driven as a binary pattern rather than
//  as three independent switches, which makes the heat setting one
//  number instead of three flags: level 1 lights the first element,
//  level 2 the first two, level 3 all three. The fan is a fourth
//  relay and is not part of that pattern.
//
//  This module owns the relay writes for the whole firmware, so the
//  elements are never driven from two places at once. The stored
//  level is what the display shows and what MANUAL re-applies.
// ============================================================

namespace
{
int g_heatLevel = 0;
bool g_fanOn = false;

void writeHeatRelays(bool first, bool second, bool third)
{
    digitalWrite(PIN_HEAT_1, first ? HIGH : LOW);
    digitalWrite(PIN_HEAT_2, second ? HIGH : LOW);
    digitalWrite(PIN_HEAT_3, third ? HIGH : LOW);
}
} // namespace

void heatControlInit()
{
    pinMode(PIN_HEAT_1, OUTPUT);
    pinMode(PIN_HEAT_2, OUTPUT);
    pinMode(PIN_HEAT_3, OUTPUT);
    pinMode(PIN_FAN, OUTPUT);

    // Elements and fan off before anything else runs, so an
    // interrupted boot cannot leave the drier heating.
    heatControlSetLevel(HEAT_LEVEL_MIN);
    heatControlSetFan(false);
}

int heatControlLevel()
{
    return g_heatLevel;
}

bool heatControlFanOn()
{
    return g_fanOn;
}

void heatControlSetLevel(int heatLevel)
{
    switch (heatLevel)
    {
    case 1:
        writeHeatRelays(true, false, false);
        break;
    case 2:
        writeHeatRelays(true, true, false);
        break;
    case 3:
        writeHeatRelays(true, true, true);
        break;
    default:
        writeHeatRelays(false, false, false);
        break;
    }

    g_heatLevel = heatLevel;
}

void heatControlSetFan(bool fanOn)
{
    digitalWrite(PIN_FAN, fanOn ? HIGH : LOW);
    g_fanOn = fanOn;
}

void heatControlApplyAuto(float temperatureC)
{
    heatControlSetFan(true);

    if (temperatureC > AUTO_HEAT_OFF_ABOVE_C)
    {
        heatControlSetLevel(0);
    }
    else if (temperatureC > AUTO_HEAT_LEVEL1_ABOVE_C)
    {
        heatControlSetLevel(1);
    }
    else if (temperatureC > AUTO_HEAT_LEVEL2_ABOVE_C)
    {
        heatControlSetLevel(2);
    }
    else
    {
        heatControlSetLevel(3);
    }
}
