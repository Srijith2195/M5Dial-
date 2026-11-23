/**
 * @file app_timer.cpp
 * @brief Countdown Timer with Buzzer Alarm for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#include "app_timer.h"
#include "../common_define.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>

using namespace MOONCAKE::USER_APP;

int64_t TimerApp::_remaining_us()
{
    switch (_data.state)
    {
        case TIMER::RUNNING:
        {
            int64_t r = _data.end_us - esp_timer_get_time();
            return (r > 0) ? r : 0;
        }
        case TIMER::PAUSED:
            return _data.remain_us;
        default:
            return (int64_t)_data.set_seconds * 1000000LL;
    }
}

void TimerApp::_start_pause()
{
    if (_data.hal == nullptr) return;

    switch (_data.state)
    {
        case TIMER::IDLE:
            if (_data.set_seconds <= 0) return;
            _data.end_us = esp_timer_get_time() + (int64_t)_data.set_seconds * 1000000LL;
            _data.state = TIMER::RUNNING;
            _data.hal->buzz.tone(3600, 30);
            break;
        case TIMER::RUNNING:
            _data.remain_us = _remaining_us();
            _data.state = TIMER::PAUSED;
            _data.hal->buzz.tone(2800, 25);
            break;
        case TIMER::PAUSED:
            _data.end_us = esp_timer_get_time() + _data.remain_us;
            _data.state = TIMER::RUNNING;
            _data.hal->buzz.tone(3600, 30);
            break;
        case TIMER::FINISHED:
            _data.state = TIMER::IDLE;
            _data.hal->buzz.noTone();
            break;
    }
    _render();
}

void TimerApp::_reset()
{
    if (_data.hal == nullptr) return;
    _data.state = TIMER::IDLE;
    _data.hal->buzz.tone(2400, 30);
    _render();
}

void TimerApp::_render()
{
    if (_data.hal == nullptr) return;

    if (_data.state == TIMER::FINISHED && _data.alarm_flash) {
        _data.hal->canvas->fillScreen((uint32_t)0x550000);
    } else {
        _data.hal->canvas->fillScreen(TFT_BLACK);
    }

    int64_t r = _remaining_us();
    int32_t s = (int32_t)((r + 999999) / 1000000);
    int m_part = s / 60;
    int s_part = s % 60;

    // 1. Progress Arc
    int32_t pct = (_data.set_seconds > 0) ? (int32_t)(r * 100 / ((int64_t)_data.set_seconds * 1000000LL)) : 0;
    pct = std::max(0, std::min(100, (int)pct));
    int arc_deg = pct * 360 / 100;

    _data.hal->canvas->drawArc(120, 120, 114, 118, 0, 360, (uint32_t)0x222222);
    if (arc_deg > 0) {
        uint32_t arc_col = (_data.state == TIMER::FINISHED) ? (uint32_t)0xFF1744 : (uint32_t)0xFF5252;
        _data.hal->canvas->drawArc(120, 120, 114, 118, 270, (270 + arc_deg) % 360, arc_col);
    }

    // 2. Title Pill
    _data.hal->canvas->fillRoundRect(70, 12, 100, 22, 11, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawRoundRect(70, 12, 100, 22, 11, (uint32_t)0x333333);
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setTextColor((uint32_t)0xFF5252, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawString("TIMER", 120, 23);

    // 3. Time Display
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", m_part, s_part);
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    _data.hal->canvas->setFont(&fonts::Font4);
    _data.hal->canvas->drawString(time_str, 120, 85);

    // 4. Status Hint
    _data.hal->canvas->setFont(&fonts::Font0);
    const char* hint_txt = "Rotate: Adjust Time";
    if (_data.state == TIMER::RUNNING) hint_txt = "Counting Down...";
    else if (_data.state == TIMER::PAUSED) hint_txt = "Paused - Tap to Resume";
    else if (_data.state == TIMER::FINISHED) hint_txt = "TIME'S UP! Tap to Stop";

    _data.hal->canvas->setTextColor((_data.state == TIMER::FINISHED) ? (uint32_t)0xFFEA00 : (uint32_t)0x888888, TFT_BLACK);
    _data.hal->canvas->drawString(hint_txt, 120, 122);

    // 5. Quick Preset Pills (1m, 3m, 5m, 10m)
    static const int presets[] = { 60, 180, 300, 600 };
    static const char* p_names[] = { "1m", "3m", "5m", "10m" };
    int px_start = 38;
    for (int i = 0; i < 4; i++) {
        int bx = px_start + i * 44;
        bool is_cur = (_data.state == TIMER::IDLE && _data.set_seconds == presets[i]);
        uint32_t bg = is_cur ? (uint32_t)0xFF5252 : (uint32_t)0x222226;
        _data.hal->canvas->fillRoundRect(bx, 142, 38, 24, 6, bg);
        _data.hal->canvas->setTextColor(is_cur ? TFT_WHITE : (uint32_t)0xAAAAAA, bg);
        _data.hal->canvas->drawString(p_names[i], bx + 19, 154);
    }

    // 6. Bottom Controls: Reset & Play/Pause
    _data.hal->canvas->fillRoundRect(42, 182, 68, 30, 15, (uint32_t)0x2A2A2E);
    _data.hal->canvas->setTextColor(TFT_WHITE, (uint32_t)0x2A2A2E);
    _data.hal->canvas->drawString("Reset", 76, 197);

    uint32_t btn_col = (_data.state == TIMER::RUNNING) ? (uint32_t)0xFF9800 : (uint32_t)0x2E7D32;
    const char* btn_lbl = (_data.state == TIMER::RUNNING) ? "Pause" : "Start";
    _data.hal->canvas->fillRoundRect(130, 182, 68, 30, 15, btn_col);
    _data.hal->canvas->setTextColor(TFT_WHITE, btn_col);
    _data.hal->canvas->drawString(btn_lbl, 164, 197);

    _data.hal->canvas->pushSprite(0, 0);
}

void TimerApp::onSetup()
{
    setAppName("Timer");
    setAllowBgRunning(false);

    TIMER::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void TimerApp::onCreate()
{
    _log("onCreate");
    _render();
}

void TimerApp::onRunning()
{
    // 1. Check countdown finish
    if (_data.state == TIMER::RUNNING && esp_timer_get_time() >= _data.end_us)
    {
        _data.state = TIMER::FINISHED;
        _data.last_alarm_beep = 0;
        _render();
    }

    // 2. Alarm sound & flash when finished
    if (_data.state == TIMER::FINISHED)
    {
        if (millis() - _data.last_alarm_beep > 400)
        {
            _data.last_alarm_beep = millis();
            _data.alarm_flash = !_data.alarm_flash;
            _data.hal->buzz.tone(3200, 120);
            _render();
        }
    }

    // 3. Encoder Knob Click / Long Press
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
        _start_pause();
    }

    // 4. Dial rotation: adjust timer duration (when IDLE)
    if (_data.hal->encoder.wasMoved(true))
    {
        if (_data.state == TIMER::IDLE)
        {
            _data.hal->buzz.tone(3600, 12);
            int step = (_data.set_seconds >= 300) ? 60 : (_data.set_seconds >= 60 ? 30 : 15);
            if (_data.hal->encoder.getDirection() < 1) {
                _data.set_seconds = std::min<int32_t>(3600, _data.set_seconds + step);
            } else {
                _data.set_seconds = std::max<int32_t>(15, _data.set_seconds - step);
            }
            _render();
        }
    }

    // 5. Touch Handling
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
            // If alarm ringing, tap anywhere stops it
            if (_data.state == TIMER::FINISHED)
            {
                _data.state = TIMER::IDLE;
                _data.hal->buzz.noTone();
                _render();
                return;
            }

            // Presets row (y between 136 and 170)
            if (_data.state == TIMER::IDLE && start_y >= 136 && start_y <= 170)
            {
                static const int presets[] = { 60, 180, 300, 600 };
                for (int i = 0; i < 4; i++) {
                    int bx = 38 + i * 44;
                    if (start_x >= bx && start_x <= bx + 40) {
                        _data.set_seconds = presets[i];
                        _data.hal->buzz.tone(3800, 18);
                        _render();
                        return;
                    }
                }
            }

            // Bottom Buttons (Reset / Start)
            if (start_y >= 175) {
                if (start_x < 115) {
                    _reset();
                } else if (start_x > 125) {
                    _start_pause();
                }
            } else {
                _start_pause();
            }
        }
    }

    // 6. Refresh while running (every 250ms)
    if (_data.state == TIMER::RUNNING && (millis() - _data.last_render > 250))
    {
        _data.last_render = millis();
        _render();
    }

    delay(10);
}

void TimerApp::onDestroy()
{
    _log("onDestroy");
    if (_data.hal != nullptr)
    {
        _data.hal->buzz.noTone();
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_timer_app(HAL::HAL* hal)
{
    TimerApp app;
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
