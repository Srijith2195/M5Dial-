/**
 * @file app_flashlight.cpp
 * @brief Multi-color Torch & Dimmer for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#include "app_flashlight.h"
#include "../common_define.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>

using namespace MOONCAKE::USER_APP;

static const uint32_t s_torch_colors[] = { (uint32_t)0xFFFFFF, (uint32_t)0xFFD39B, (uint32_t)0xFF1744 };
static const char* s_torch_names[] = { "White", "Warm Amber", "Tactical Red" };

void FlashlightApp::_apply()
{
    if (_data.hal == nullptr) return;

    uint32_t col = s_torch_colors[_data.color_mode];
    _data.hal->canvas->fillScreen(col);

    // Subtle bottom pill with name & brightness
    uint32_t pill_bg = (_data.color_mode == 2) ? (uint32_t)0x330000 : (uint32_t)0x222222;
    uint32_t txt_col = (_data.color_mode == 2) ? (uint32_t)0xFFAAAA : TFT_WHITE;

    _data.hal->canvas->fillRoundRect(50, 186, 140, 26, 13, pill_bg);
    _data.hal->canvas->drawRoundRect(50, 186, 140, 26, 13, (uint32_t)0x555555);

    char label[32];
    snprintf(label, sizeof(label), "%s  %d%%", s_torch_names[_data.color_mode], _data.brightness);
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setTextColor(txt_col, pill_bg);
    _data.hal->canvas->setFont(&fonts::Font0);
    _data.hal->canvas->drawString(label, 120, 199);

    _data.hal->canvas->pushSprite(0, 0);

    // Apply hardware backlight brightness
    _data.hal->display.setBrightness((_data.brightness * 255) / 100);
}

void FlashlightApp::_next_color()
{
    _data.color_mode = (_data.color_mode + 1) % 3;
    if (_data.hal != nullptr) {
        _data.hal->buzz.tone(3600, 20);
    }
    _apply();
}

void FlashlightApp::onSetup()
{
    setAppName("Flashlight");
    setAllowBgRunning(false);

    FLASHLIGHT::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void FlashlightApp::onCreate()
{
    _log("onCreate");
    if (_data.hal != nullptr) {
        _data.old_brightness = _data.hal->display.getBrightness();
        _data.brightness = 100;
    }
    _apply();
}

void FlashlightApp::onRunning()
{
    // 1. Dial knob rotation: Adjust brightness level
    if (_data.hal->encoder.wasMoved(true))
    {
        if (_data.hal->encoder.getDirection() < 1) {
            _data.brightness = std::min(100, _data.brightness + 5);
        } else {
            _data.brightness = std::max(5, _data.brightness - 5);
        }
        _apply();
    }

    // 2. Encoder knob click: next color mode / long press: exit
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
        _next_color();
    }

    // 3. Touch Screen: tap -> next color, swipe left -> exit
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
            _next_color();
        }
    }

    delay(15);
}

void FlashlightApp::onDestroy()
{
    _log("onDestroy");
    if (_data.hal != nullptr)
    {
        _data.hal->display.setBrightness(_data.old_brightness);
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_flashlight_app(HAL::HAL* hal)
{
    FlashlightApp app;
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
