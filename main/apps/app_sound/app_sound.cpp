/**
 * @file app_sound.cpp
 * @brief Sound Configuration App for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#include "app_sound.h"
#include "../common_define.h"
#include <cstdlib>

using namespace MOONCAKE::USER_APP;

void SoundApp::_render()
{
    if (_data.hal == nullptr) return;

    _data.hal->canvas->fillScreen(TFT_BLACK);

    // 1. Top Header Pill
    _data.hal->canvas->fillRoundRect(50, 12, 140, 22, 11, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawRoundRect(50, 12, 140, 22, 11, (uint32_t)0x333333);
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setTextColor((uint32_t)0x00E5FF, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawString("SOUND SETTINGS", 120, 23);

    // 2. Large Center Graphic
    int cx = 120, cy = 100;
    if (_data.sound_enabled) {
        // Sound Enabled
        _data.hal->canvas->fillCircle(cx, cy, 44, (uint32_t)0x0B2E13);
        _data.hal->canvas->drawCircle(cx, cy, 44, (uint32_t)0x00E676);
        _data.hal->canvas->drawCircle(cx, cy, 45, (uint32_t)0x00E676);

        // Speaker cone
        _data.hal->canvas->fillRect(cx - 20, cy - 8, 10, 16, TFT_WHITE);
        _data.hal->canvas->fillTriangle(cx - 10, cy - 8, cx + 4, cy - 18, cx + 4, cy + 18, TFT_WHITE);
        _data.hal->canvas->fillTriangle(cx - 10, cy - 8, cx - 10, cy + 8, cx + 4, cy + 18, TFT_WHITE);

        // Radiating sound wave arcs
        _data.hal->canvas->drawArc(cx, cy, 18, 20, 315, 45, (uint32_t)0x00FF66);
        _data.hal->canvas->drawArc(cx, cy, 28, 30, 315, 45, (uint32_t)0x00FF66);

        // Status text
        _data.hal->canvas->setTextColor((uint32_t)0x00FF66, TFT_BLACK);
        _data.hal->canvas->drawString("SOUND: ON", 120, 156);
    } else {
        // Sound Muted
        _data.hal->canvas->fillCircle(cx, cy, 44, (uint32_t)0x2E0B0B);
        _data.hal->canvas->drawCircle(cx, cy, 44, (uint32_t)0xFF1744);
        _data.hal->canvas->drawCircle(cx, cy, 45, (uint32_t)0xFF1744);

        // Speaker cone (dim)
        _data.hal->canvas->fillRect(cx - 20, cy - 8, 10, 16, (uint32_t)0x777777);
        _data.hal->canvas->fillTriangle(cx - 10, cy - 8, cx + 4, cy - 18, cx + 4, cy + 18, (uint32_t)0x777777);
        _data.hal->canvas->fillTriangle(cx - 10, cy - 8, cx - 10, cy + 8, cx + 4, cy + 18, (uint32_t)0x777777);

        // Bold Red strike-through slash
        _data.hal->canvas->drawLine(cx - 26, cy - 26, cx + 26, cy + 26, (uint32_t)0xFF1744);
        _data.hal->canvas->drawLine(cx - 25, cy - 26, cx + 27, cy + 26, (uint32_t)0xFF1744);
        _data.hal->canvas->drawLine(cx - 27, cy - 26, cx + 25, cy + 26, (uint32_t)0xFF1744);

        // Status text
        _data.hal->canvas->setTextColor((uint32_t)0xFF5252, TFT_BLACK);
        _data.hal->canvas->drawString("SOUND: MUTED", 120, 156);
    }

    // 3. User interaction hints
    _data.hal->canvas->setTextColor((uint32_t)0x888888, TFT_BLACK);
    _data.hal->canvas->drawString("Tap or Dial to Toggle", 120, 180);

    _data.hal->canvas->setTextColor((uint32_t)0x555555, TFT_BLACK);
    _data.hal->canvas->drawString("Swipe Left: Back", 120, 204);

    _data.hal->canvas->pushSprite(0, 0);
}

void SoundApp::_toggle_sound()
{
    if (_data.hal == nullptr) return;

    _data.hal->buzz.toggle();
    _data.sound_enabled = _data.hal->buzz.isEnabled();

    if (_data.sound_enabled) {
        _data.hal->buzz.tone(3600, 40);
    }

    _render();
}

void SoundApp::onSetup()
{
    setAppName("Sound Settings");
    setAllowBgRunning(false);

    SOUND_CONFIG::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void SoundApp::onCreate()
{
    _log("onCreate");
    if (_data.hal != nullptr) {
        _data.sound_enabled = _data.hal->buzz.isEnabled();
    }
    _render();
}

void SoundApp::onRunning()
{
    // 1. Rotary dial turned -> toggle sound
    if (_data.hal->encoder.wasMoved(true))
    {
        _toggle_sound();
        delay(120);
    }

    // 2. Encoder knob clicked or long pressed
    if (!_data.hal->encoder.btn.read())
    {
        uint32_t press_start = millis();
        while (!_data.hal->encoder.btn.read())
        {
            delay(10);
            if (millis() - press_start > 650)
            {
                _data.hal->buzz.tone(2600, 30);
                while (!_data.hal->encoder.btn.read()) delay(10);
                destroyApp();
                return;
            }
        }
        _toggle_sound();
        delay(100);
    }

    // 3. Touch Screen
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

        // Swipe Left -> Back Navigate to Launcher
        if (dx < -40 && abs(dy) < 55)
        {
            _data.hal->buzz.tone(3000, 30);
            destroyApp();
            return;
        }

        // Tap -> Toggle Sound
        if (abs(dx) <= 25 && abs(dy) <= 25 && duration < 600)
        {
            _toggle_sound();
        }
    }

    delay(15);
}

void SoundApp::onDestroy()
{
    _log("onDestroy");
    if (_data.hal != nullptr)
    {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_sound_app(HAL::HAL* hal)
{
    SoundApp app;
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
