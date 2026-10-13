#pragma once

// Top-level flow: everything the drier does, in order.
//
// drierInit()   boot: relay outputs off, display, air sensor,
//               Wi-Fi association and the first cloud attempt
// drierUpdate() one pass: cloud retry, keypad, air sensor, the
//               mode policy, the display, then the 5 s publish slot
void drierInit();
void drierUpdate();
