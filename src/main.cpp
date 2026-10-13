#include <Arduino.h>

#include "drier.h"

// ============================================================
//  Automated Food Drier — firmware entry point
//
//  This file stays a thin entry point: it hands control to the drier
//  module, so no board-specific detail leaks in here.
//
//  boot / one pass of loop()
//    drier         the flow: relays, display, sensor, Wi-Fi, modes
//    cloud_link    Wi-Fi, NTP and the Firebase Realtime Database
//    sensors       DHT22 air temperature and humidity
//    heat_control  heat level 0..3 and the fan relay
//    keypad_ui     keypad scan, operating modes and key actions
//    display       the 20x4 display and its padding rule
//
//  Wiring, cloud layout and mode behaviour are in README.md.
// ============================================================

void setup()
{
    drierInit();
}

void loop()
{
    drierUpdate();
}
