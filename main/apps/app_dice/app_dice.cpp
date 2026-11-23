/**
 * @file app_dice.cpp
 * @brief Smartwatch Two-Dice Roller App implementation
 */

#include "app_dice.h"
#include "../common_define.h"
#include <esp_random.h>
#include <cstdio>
#include <algorithm>

using namespace MOONCAKE::USER_APP;

/* Pip layout on a 3x3 grid: 7 slots */
static const int8_t s_slot_offsets[7][2] = {
    {-20, -20}, // 0: Top-Left
    { 20, -20}, // 1: Top-Right
    {-20,   0}, // 2: Mid-Left
    {  0,   0}, // 3: Center
    { 20,   0}, // 4: Mid-Right
    {-20,  20}, // 5: Bottom-Left
    { 20,  20}  // 6: Bottom-Right
};

static const uint8_t s_faces[7] = {
    0,
    0x08, // 1: center
    0x41, // 2: TL, BR
    0x49, // 3: TL, C, BR
    0x63, // 4: 4 corners
    0x6B, // 5: 4 corners + center
    0x77  // 6: 4 corners + mid sides
};

void Dice::_draw_die(int cx, int cy, int val, uint32_t pip_color, int angle)
{
    auto canvas = _data.hal->canvas;
    const int DIE_SIZE = 70;
    const int HALF_SIZE = DIE_SIZE / 2;

    // Draw white rounded die body
    canvas->fillSmoothRoundRect(cx - HALF_SIZE, cy - HALF_SIZE, DIE_SIZE, DIE_SIZE, 14, TFT_WHITE);
    canvas->drawRoundRect(cx - HALF_SIZE, cy - HALF_SIZE, DIE_SIZE, DIE_SIZE, 14, (uint32_t)0xE2E8F0);

    // Draw pips according to bitmask
    uint8_t mask = s_faces[std::max(1, std::min(6, val))];
    for (int i = 0; i < 7; i++) {
        if (mask & (1 << i)) {
            int px = cx + s_slot_offsets[i][0];
            int py = cy + s_slot_offsets[i][1];
            canvas->fillSmoothCircle(px, py, 6, pip_color);
        }
    }
}

void Dice::_render()
{
    auto canvas = _data.hal->canvas;
    canvas->fillScreen(TFT_BLACK);
    canvas->setTextDatum(textdatum_t::middle_center);

    // 1. Title (clean smartwatch display without on-screen back button)
    canvas->setFont(GUI_FONT_CN_BIG);
    canvas->setTextColor((uint32_t)0xFF375F, TFT_BLACK);
    canvas->drawString("DICE", 120, 26);

    // 2. Two Dice
    // Left Die: Black pips
    _draw_die(76, 95, _data.value[0], (uint32_t)0x0F172A, _data.angle[0]);
    // Right Die: Crimson pips (0xFF375F)
    _draw_die(164, 95, _data.value[1], (uint32_t)0xFF375F, _data.angle[1]);

    // 3. Total Sum Display
    canvas->setFont(GUI_FONT_CN_BIG);
    if (_data.is_rolling) {
        canvas->setTextColor((uint32_t)0xFCD34D, TFT_BLACK);
        canvas->drawString("...", 120, 160);
    } else {
        char total_str[32];
        int total = _data.value[0] + _data.value[1];
        snprintf(total_str, sizeof(total_str), "%d", total);

        // Circular background badge for total
        canvas->fillSmoothCircle(120, 160, 22, (uint32_t)0x1E293B);
        canvas->drawCircle(120, 160, 22, (uint32_t)0x475569);
        canvas->setTextColor(TFT_WHITE, (uint32_t)0x1E293B);
        canvas->drawString(total_str, 120, 160);
    }

    // 4. Bottom Hint
    canvas->setFont(GUI_FONT_CN_SMALL);
    canvas->setTextColor((uint32_t)0x64748B, TFT_BLACK);
    canvas->drawString("Tap, Press or Spin | Swipe: Back", 120, 208);

    canvas->pushSprite(0, 0);
}

void Dice::_roll()
{
    if (_data.is_rolling) return;
    _data.hal->buzz.tone(3200, 20);
    _data.is_rolling = true;
    _data.roll_frames = 10;
    _data.last_frame_time = millis();
}

void Dice::onSetup()
{
    setAppName("Dice");
    setAllowBgRunning(false);

    Dice::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void Dice::onCreate()
{
    _data.value[0] = 6;
    _data.value[1] = 6;
    _render();
}

void Dice::onRunning()
{
    // 1. Animation Loop if rolling
    if (_data.is_rolling) {
        if (millis() - _data.last_frame_time >= 70) {
            _data.last_frame_time = millis();
            _data.value[0] = 1 + (esp_random() % 6);
            _data.value[1] = 1 + (esp_random() % 6);
            _data.angle[0] = (esp_random() % 20) - 10;
            _data.angle[1] = (esp_random() % 20) - 10;

            // Click tone during roll
            _data.hal->buzz.tone(900 + (esp_random() % 600), 25);
            _render();

            _data.roll_frames--;
            if (_data.roll_frames <= 0) {
                _data.is_rolling = false;
                _data.angle[0] = 0;
                _data.angle[1] = 0;
                // Final celebratory chime
                _data.hal->buzz.tone(2200, 80);
                _render();
            }
        }
        return;
    }

    // 2. Rotary Dial rotation triggers roll
    if (_data.hal->encoder.wasMoved(true)) {
        _roll();
        return;
    }

    // 3. Encoder Button click triggers roll, long-press exits
    if (!_data.hal->encoder.btn.read()) {
        uint32_t press_start = millis();
        bool is_long_press = false;

        while (!_data.hal->encoder.btn.read()) {
            delay(10);
            if (millis() - press_start > 700) {
                _data.hal->buzz.tone(2800, 40);
                is_long_press = true;
                while (!_data.hal->encoder.btn.read()) delay(10);
                destroyApp();
                return;
            }
        }

        if (!is_long_press) {
            _roll();
            return;
        }
    }

    // 4. Touch Handling: Swipe Left / Swipe Right -> Back to Previous Screen; Tap anywhere -> Roll
    if (_data.hal->tp.isTouched()) {
        _data.hal->tp.update();
        auto pt = _data.hal->tp.getTouchPointBuffer();
        int start_x = pt.x;
        int start_y = pt.y;
        int last_x = pt.x;
        int last_y = pt.y;
        uint32_t start_time = millis();

        while (_data.hal->tp.isTouched()) {
            _data.hal->tp.update();
            last_x = _data.hal->tp.getTouchPointBuffer().x;
            last_y = _data.hal->tp.getTouchPointBuffer().y;
            delay(10);
        }

        int dx = last_x - start_x;
        int dy = last_y - start_y;
        uint32_t duration = millis() - start_time;

        // Swipe Left -> Go Back to Previous Screen (Launcher)
        if (dx < -40 && abs(dy) < 55) {
            _data.hal->buzz.tone(3000, 30);
            destroyApp();
            return;
        }

        // Swipe Right or Tap -> Roll Dice
        if (dx > 40 && abs(dy) < 55) {
            _roll();
            return;
        }

        // Tap anywhere on screen -> Roll Dice
        if (abs(dx) <= 25 && abs(dy) <= 25 && duration < 600) {
            _roll();
            return;
        }
    }

    delay(15);
}

void Dice::onDestroy()
{
    if (_data.hal != nullptr) {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_dice_app(HAL::HAL* hal)
{
    Dice app;
    app.setUserData(hal);
    app.onSetup();
    app.onCreate();
    while (1) {
        app.onRunning();
        if (app.isGoingDestroy()) {
            app.resetGoingDestroyFlag();
            app.onDestroy();
            break;
        }
    }
}
