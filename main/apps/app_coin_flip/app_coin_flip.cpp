/**
 * @file app_coin_flip.cpp
 * @brief 3D Animated Coin Flip Decision Maker for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#include "app_coin_flip.h"
#include "../common_define.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdlib>

using namespace MOONCAKE::USER_APP;

void CoinFlipApp::_flip()
{
    if (_data.is_flipping || _data.hal == nullptr) return;

    _data.is_flipping = true;

    // Pick final outcome with true random
    _data.current_side = (rand() % 2 == 0) ? COIN_FLIP::HEADS : COIN_FLIP::TAILS;

    int total_frames = 20 + (rand() % 6);
    for (int f = 0; f < total_frames; f++) {
        _data.flip_frame = f;

        // Perspective spinning sound ticks
        _data.hal->buzz.tone(2000 + (f % 5) * 350, 12);

        _render();
        delay(35);
    }

    _data.is_flipping = false;
    if (_data.current_side == COIN_FLIP::HEADS) {
        _data.heads_count++;
    } else {
        _data.tails_count++;
    }

    // Landing celebration chime
    _data.hal->buzz.tone(3600, 50);
    delay(60);
    _data.hal->buzz.tone(4500, 70);

    _render();
}

void CoinFlipApp::_render()
{
    if (_data.hal == nullptr) return;

    _data.hal->canvas->fillScreen(TFT_BLACK);

    // 1. Header Pill
    _data.hal->canvas->fillRoundRect(65, 12, 110, 22, 11, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawRoundRect(65, 12, 110, 22, 11, (uint32_t)0x333333);
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setTextColor((uint32_t)0xFFC107, (uint32_t)0x1A1A1A);
    _data.hal->canvas->setFont(&fonts::Font0);
    _data.hal->canvas->drawString("COIN FLIP", 120, 23);

    // 2. 3D Animated Gold Coin
    int cx = 120, cy = 105;
    float scale_y = 1.0f;
    bool show_face = true;

    if (_data.is_flipping) {
        float angle = _data.flip_frame * 0.45f;
        scale_y = fabs(cosf(angle));
        if (scale_y < 0.12f) scale_y = 0.12f;
        show_face = (sinf(angle) > 0);
    }

    int rx = 48;
    int ry = std::max(6, (int)(48 * scale_y));

    // Outer rim & edge depth
    _data.hal->canvas->fillEllipse(cx, cy + 3, rx, ry, (uint32_t)0xB28704); // coin edge shadow
    _data.hal->canvas->fillEllipse(cx, cy, rx, ry, (uint32_t)0xFFC107);     // gold face
    _data.hal->canvas->drawEllipse(cx, cy, rx, ry, (uint32_t)0xFFF8E1);     // rim highlight

    if (ry >= 16) {
        _data.hal->canvas->drawEllipse(cx, cy, rx - 7, (int)((rx - 7) * scale_y), (uint32_t)0xE6A100);

        COIN_FLIP::Side_t side_to_draw = _data.is_flipping ? (show_face ? COIN_FLIP::HEADS : COIN_FLIP::TAILS) : _data.current_side;

        if (side_to_draw == COIN_FLIP::HEADS) {
            // Crown / Eagle glyph for HEADS
            _data.hal->canvas->setTextColor((uint32_t)0x7B5500, (uint32_t)0xFFC107);
            _data.hal->canvas->setFont(&fonts::Font2);
            _data.hal->canvas->drawString("H", cx, cy);
        } else {
            // Number '1' / Laurel for TAILS
            _data.hal->canvas->setTextColor((uint32_t)0x7B5500, (uint32_t)0xFFC107);
            _data.hal->canvas->setFont(&fonts::Font2);
            _data.hal->canvas->drawString("T", cx, cy);
        }
    }

    // 3. Result Label
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setFont(&fonts::Font2);
    if (_data.is_flipping) {
        _data.hal->canvas->setTextColor((uint32_t)0xAAAAAA, TFT_BLACK);
        _data.hal->canvas->drawString("FLIPPING...", 120, 162);
    } else {
        if (_data.current_side == COIN_FLIP::HEADS) {
            _data.hal->canvas->setTextColor((uint32_t)0xFFEA00, TFT_BLACK);
            _data.hal->canvas->drawString("HEADS", 120, 162);
        } else {
            _data.hal->canvas->setTextColor((uint32_t)0x00E5FF, TFT_BLACK);
            _data.hal->canvas->drawString("TAILS", 120, 162);
        }
    }

    // 4. Session Statistics Pill
    _data.hal->canvas->setFont(&fonts::Font0);
    char stat_buf[48];
    snprintf(stat_buf, sizeof(stat_buf), "H: %d   |   T: %d", _data.heads_count, _data.tails_count);
    _data.hal->canvas->fillRoundRect(55, 184, 130, 22, 11, (uint32_t)0x1A1A1A);
    _data.hal->canvas->setTextColor((uint32_t)0x888888, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawString(stat_buf, 120, 195);

    // 5. Instruction
    _data.hal->canvas->setTextColor((uint32_t)0x555555, TFT_BLACK);
    _data.hal->canvas->drawString("Tap or Dial: Flip  |  Swipe: Exit", 120, 217);

    _data.hal->canvas->pushSprite(0, 0);
}

void CoinFlipApp::onSetup()
{
    setAppName("Coin Flip");
    setAllowBgRunning(false);

    COIN_FLIP::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void CoinFlipApp::onCreate()
{
    _log("onCreate");
    _render();
}

void CoinFlipApp::onRunning()
{
    // 1. Dial knob rotation: Flip coin
    if (_data.hal->encoder.wasMoved(true))
    {
        _flip();
        delay(100);
    }

    // 2. Dial knob click: Flip / long press: exit
    bool btn_down = (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0);
    if (btn_down)
    {
        uint32_t press_start = millis();
        while (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0)
        {
            delay(10);
            if (millis() - press_start > 650)
            {
                _data.hal->buzz.tone(2600, 30);
                while (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0) delay(10);
                destroyApp();
                return;
            }
        }
        _flip();
    }

    // 3. Touch Screen: Tap to Flip / Swipe Left to Exit
    if (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        auto pt = _data.hal->tp.getTouchPointBuffer();
        int start_x = pt.x, start_y = pt.y, last_x = pt.x, last_y = pt.y;
        uint32_t start_time = millis();

        while (_data.hal->tp.isTouched())
        {
            _data.hal->tp.update();
            last_x = _data.hal->tp.getTouchPointBuffer().x;
            last_y = _data.hal->tp.getTouchPointBuffer().y;
            delay(10);
        }

        int dx = last_x - start_x;
        int dy = last_y - start_y;
        uint32_t duration = millis() - start_time;

        // Swipe Left -> Back Navigate
        if (dx < -40 && abs(dy) < 55)
        {
            _data.hal->buzz.tone(3000, 30);
            destroyApp();
            return;
        }

        // Tap
        if (abs(dx) <= 25 && abs(dy) <= 25 && duration < 600)
        {
            _flip();
        }
    }

    delay(20);
}

void CoinFlipApp::onDestroy()
{
    _log("onDestroy");
    if (_data.hal != nullptr)
    {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_coin_flip_app(HAL::HAL* hal)
{
    CoinFlipApp app;
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
