#include "display.h"

#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#include "config.h"

// ============================================================
//  20x4 I2C display
//
//  The four rows are redrawn on every pass of loop(). Each row is
//  written as exactly LCD_COLUMNS characters, which is what stops a
//  shorter line from leaving the tail of the previous one behind it —
//  the padding is why the display does not have to be cleared, and
//  why it does not flicker.
//
//  A row longer than the display is clipped here rather than allowed
//  to run: the controller carries the overflow onto the next line by
//  itself, so an unpadded overlong row would eat the row below.
// ============================================================

namespace
{
LiquidCrystal_I2C g_lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

void writeRow(uint8_t row, const String& text)
{
    char line[LCD_COLUMNS + 1];

    const size_t length = min(static_cast<size_t>(text.length()), static_cast<size_t>(LCD_COLUMNS));
    memcpy(line, text.c_str(), length);
    memset(line + length, ' ', LCD_COLUMNS - length);
    line[LCD_COLUMNS] = '\0';

    g_lcd.setCursor(0, row);
    g_lcd.print(line);
}
} // namespace

void displayInit()
{
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    g_lcd.init();
    g_lcd.backlight();
    g_lcd.clear();

    writeRow(0, "Initializing...");
    delay(SPLASH_HOLD_MS);
}

void displayShowConnecting()
{
    g_lcd.clear();

    writeRow(0, "   FOOD DRIER    ");
    writeRow(1, "Connecting WiFi...");
}

void displayRender(bool online, Mode mode, int heatLevel, bool fanOn, float temperatureC, float humidityPct, char lastKey, bool keyEcho)
{
    writeRow(0, "    FOOD DRIER    ");

    String statusRow = online ? " ONLINE " : " OFFLINE";
    statusRow += (mode == AUTO) ? " | AUTO   " : " | MANUAL ";
    writeRow(1, statusRow);

    String outputRow = "Heat:";
    outputRow += String(heatLevel);
    outputRow += "  Fan:";
    outputRow += fanOn ? "ON " : "OFF";
    writeRow(2, outputRow);

    // The key echo is the one field that changes on a key press, and
    // it expires on its own so the row goes back to showing '-'.
    String readingRow = "T:";
    readingRow += String(temperatureC, SENSOR_READING_DECIMALS);
    readingRow += "C H:";
    readingRow += String(humidityPct, SENSOR_READING_DECIMALS);
    readingRow += "% K:";
    readingRow += (keyEcho ? lastKey : '-');
    writeRow(3, readingRow);
}
