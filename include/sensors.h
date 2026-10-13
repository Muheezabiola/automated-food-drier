#pragma once

// DHT22 air temperature and humidity.
//
// The sensor is read on a timer and its last good pair is cached,
// because a DHT22 cannot answer faster than once every
// DHT_REFRESH_MS and answers NaN while it is still busy. Both halves
// of a reading come from the same conversion, so a half-answer is
// discarded along with the other half rather than being mixed with
// the previous reading.
void sensorsInit();

// One pass of the read timer.
void sensorsUpdate();

// The cached readings in °C and %, 0.0 until the first good one.
float sensorsTemperatureC();
float sensorsHumidityPct();
