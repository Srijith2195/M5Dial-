/**
 * @file app_lcd_test.cpp
 * @brief Smooth Freehand Drawing Pad for M5Dial
 * @version 0.2
 * @date 2026-11-17
 */
#include "app_lcd_test.h"
#include "../common_define.h"
#include <cmath>
#include <cstdlib>

using namespace MOONCAKE::USER_APP;

struct PaletteItem {
    uint32_t color;
    const char* name;
    int radius;
};

static const PaletteItem s_palette[] = {
    { (uint32_t)0x00E5FF, "Cyan",    3 },
    { (uint32_t)0x00FF66, "Green",   3 },
    { (uint32_t)0xFFEA00, "Yellow",  3 },
    { (uint32_t)0xFF6D00, "Orange",  3 },
    { (uint32_t)0xFF1744, "Red",     3 },
    { (uint32_t)0xF50057, "Pink",    3 },
    { (uint32_t)0xD500F9, "Purple",  3 },
    { (uint32_t)0x2979FF, "Blue",    3 },
    { (uint32_t)0xFFFFFF, "White",   3 },
    { (uint32_t)0x000000, "Eraser", 12 }
};
static const int s_palette_size = sizeof(s_palette) / sizeof(s_palette[0]);

static const int s_badge_x = 70;
static const int s_badge_y = 12;
static const int s_badge_w = 100;
static const int s_badge_h = 24;
static uint16_t s_badge_backup[100 * 24];
static bool s_has_backup = false;

void LCD_Test::_restore_badge_area()
{
    if (s_has_backup && _data.hal != nullptr) {
        _data.hal->canvas->pushImage(s_badge_x, s_badge_y, s_badge_w, s_badge_h, s_badge_backup);
        _data.hal->canvas->pushSprite(0, 0);
        s_has_backup = false;
        _data.badge_until = 0;
    }
}

void LCD_Test::_show_color_badge()
{
    if (_data.hal == nullptr) return;

    if (!s_has_backup) {
        _data.hal->canvas->readRect(s_badge_x, s_badge_y, s_badge_w, s_badge_h, s_badge_backup);
        s_has_backup = true;
    }

    const auto& item = s_palette[_data.color_index];

    _data.hal->canvas->fillRoundRect(s_badge_x, s_badge_y, s_badge_w, s_badge_h, 12, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawRoundRect(s_badge_x, s_badge_y, s_badge_w, s_badge_h, 12, (uint32_t)0x424242);

    if (_data.color_index == s_palette_size - 1) {
        _data.hal->canvas->drawCircle(s_badge_x + 14, s_badge_y + 12, 6, TFT_WHITE);
        _data.hal->canvas->drawLine(s_badge_x + 10, s_badge_y + 8, s_badge_x + 18, s_badge_y + 16, TFT_RED);
    } else {
        _data.hal->canvas->fillSmoothCircle(s_badge_x + 14, s_badge_y + 12, 5, item.color);
    }

    _data.hal->canvas->setTextDatum(textdatum_t::middle_left);
    _data.hal->canvas->setTextColor(TFT_WHITE, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawString(item.name, s_badge_x + 25, s_badge_y + 12);

    _data.hal->canvas->pushSprite(0, 0);
    _data.badge_until = millis() + 1400;
}

void LCD_Test::_clear_hint()
{
    if (_data.hint_until != 0 && _data.hal != nullptr) {
        _data.hal->canvas->fillRect(20, 195, 200, 25, TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
        _data.hint_until = 0;
    }
}

void LCD_Test::_draw_stroke(int x0, int y0, int x1, int y1, uint32_t color, int radius)
{
    if (_data.hal == nullptr) return;

    int dx = x1 - x0;
    int dy = y1 - y0;
    float dist = sqrtf((float)(dx * dx + dy * dy));

    if (dist < 1.0f) {
        _data.hal->canvas->fillCircle(x1, y1, radius, color);
        return;
    }

    float step = (radius >= 10) ? 3.0f : 1.5f;
    int steps = (int)ceilf(dist / step);
    if (steps < 1) steps = 1;

    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        int x = (int)roundf(x0 + dx * t);
        int y = (int)roundf(y0 + dy * t);
        _data.hal->canvas->fillCircle(x, y, radius, color);
    }
}

void LCD_Test::_clear_canvas()
{
    if (_data.hal == nullptr) return;
    s_has_backup = false;
    _data.badge_until = 0;
    _data.hint_until = 0;
    _data.hal->canvas->fillScreen(TFT_BLACK);
    _data.hal->canvas->pushSprite(0, 0);
    _show_color_badge();
}

void LCD_Test::onSetup()
{
    setAppName("Draw Pad");
    setAllowBgRunning(false);

    LCD_TEST::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void LCD_Test::onCreate()
{
    _log("onCreate");
    s_has_backup = false;
    _data.color_index = 0;

    while (_data.hal->tp.isTouched()) {
        delay(10);
    }

    _data.hal->canvas->fillScreen(TFT_BLACK);
    _show_color_badge();

    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setTextColor((uint32_t)0x666666, TFT_BLACK);
    _data.hal->canvas->drawString("Dial: Color  |  Click: Clear", 120, 205);
    _data.hal->canvas->pushSprite(0, 0);
    _data.hint_until = millis() + 3000;
}

void LCD_Test::onRunning()
{
    // 1. Dial rotation: Change color/brush
    if (_data.hal->encoder.wasMoved(true))
    {
        _data.hal->buzz.tone(3600, 15);
        if (_data.hal->encoder.getDirection() < 1)
        {
            _data.color_index = (_data.color_index + 1) % s_palette_size;
        }
        else
        {
            _data.color_index = (_data.color_index - 1 + s_palette_size) % s_palette_size;
        }
        _show_color_badge();
    }

    // 2. Encoder knob click / long press
    if (!_data.hal->encoder.btn.read())
    {
        uint32_t press_start = millis();
        while (!_data.hal->encoder.btn.read())
        {
            delay(10);
            if (millis() - press_start > 650)
            {
                // Long press -> Exit back to Launcher
                _data.hal->buzz.tone(2600, 40);
                while (!_data.hal->encoder.btn.read()) delay(10);
                destroyApp();
                return;
            }
        }
        // Short press (< 650ms) -> Clear Canvas
        _data.hal->buzz.tone(2200, 25);
        delay(40);
        _data.hal->buzz.tone(3200, 35);
        _clear_canvas();
    }

    // 3. Touch Screen Freehand Drawing
    if (_data.hal->tp.isTouched())
    {
        if (s_has_backup) _restore_badge_area();
        if (_data.hint_until != 0) _clear_hint();

        _data.hal->tp.update();
        auto pt = _data.hal->tp.getTouchPointBuffer();
        int start_x = pt.x;
        int start_y = pt.y;
        int last_x = start_x;
        int last_y = start_y;
        uint32_t start_time = millis();

        const auto& item = s_palette[_data.color_index];

        // Draw initial point
        _data.hal->canvas->fillCircle(start_x, start_y, item.radius, item.color);
        _data.hal->canvas->pushSprite(0, 0);

        // Continuous drawing loop while touch is held
        while (_data.hal->tp.isTouched())
        {
            _data.hal->tp.update();
            auto curr = _data.hal->tp.getTouchPointBuffer();
            if (curr.x != last_x || curr.y != last_y)
            {
                _draw_stroke(last_x, last_y, curr.x, curr.y, item.color, item.radius);
                _data.hal->canvas->pushSprite(0, 0);
                last_x = curr.x;
                last_y = curr.y;
            }
            delay(5);
        }

        // Check for Smartwatch navigation swipe gestures on finger release
        int dx = last_x - start_x;
        int dy = last_y - start_y;
        uint32_t duration = millis() - start_time;

        // Edge swipe left or right (< 380ms)
        bool is_edge_swipe = (start_x < 60 && dx > 50 && abs(dy) < 50 && duration < 380) ||
                             (start_x > 180 && dx < -50 && abs(dy) < 50 && duration < 380);

        // Fast fling swipe across screen (< 240ms)
        bool is_fling_swipe = (abs(dx) > 90 && abs(dy) < 40 && duration < 240);

        if (is_edge_swipe || is_fling_swipe)
        {
            _data.hal->buzz.tone(3000, 30);
            destroyApp();
            return;
        }
    }

    // 4. Auto-dismiss UI overlays
    if (_data.hint_until != 0 && millis() > _data.hint_until)
    {
        _clear_hint();
    }
    if (s_has_backup && millis() > _data.badge_until)
    {
        _restore_badge_area();
    }

    delay(10);
}

void LCD_Test::onDestroy()
{
    _log("onDestroy");
    s_has_backup = false;
    if (_data.hal != nullptr)
    {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}
