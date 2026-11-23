/**
 * @file app_stopwatch.cpp
 * @brief Precision Lap Stopwatch for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#include "app_stopwatch.h"
#include "../common_define.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace MOONCAKE::USER_APP;

int64_t StopwatchApp::_elapsed_us()
{
    return _data.acc_us + (_data.running ? (esp_timer_get_time() - _data.start_us) : 0);
}

void StopwatchApp::_format_time(char* buf, size_t max_len, int64_t us)
{
    int64_t tcs = us / 10000;
    int c = tcs % 100;
    int s = (tcs / 100) % 60;
    int m = (tcs / 6000) % 60;
    int h = tcs / 360000;
    if (h > 0) {
        snprintf(buf, max_len, "%d:%02d:%02d", h, s, c);
    } else {
        snprintf(buf, max_len, "%02d:%02d.%02d", m, s, c);
    }
}

void StopwatchApp::_start_stop()
{
    if (_data.hal == nullptr) return;

    if (_data.running) {
        _data.acc_us = _elapsed_us();
        _data.running = false;
        _data.hal->buzz.tone(3000, 25);
    } else {
        _data.start_us = esp_timer_get_time();
        _data.running = true;
        _data.hal->buzz.tone(3800, 25);
    }
    _render();
}

void StopwatchApp::_lap_reset()
{
    if (_data.hal == nullptr) return;

    if (_data.running) {
        if (_data.lap_count < 12) {
            _data.laps[_data.lap_count++] = _elapsed_us();
        } else {
            for (int i = 1; i < 12; i++) {
                _data.laps[i - 1] = _data.laps[i];
            }
            _data.laps[11] = _elapsed_us();
        }
        _data.hal->buzz.tone(4200, 30);
    } else {
        _data.acc_us = 0;
        _data.lap_count = 0;
        _data.hal->buzz.tone(2500, 25);
    }
    _render();
}

void StopwatchApp::_render()
{
    if (_data.hal == nullptr) return;

    _data.hal->canvas->fillScreen(TFT_BLACK);

    int64_t us = _elapsed_us();
    int64_t tcs = us / 10000;
    int c = tcs % 100;
    int s = (tcs / 100) % 60;
    int m = (tcs / 6000) % 60;

    // 1. Outer Progress Arc
    int angle = (int)((tcs % 6000) * 360 / 6000);
    _data.hal->canvas->drawArc(120, 120, 114, 118, 0, 360, (uint32_t)0x222222);
    if (angle > 0) {
        _data.hal->canvas->drawArc(120, 120, 114, 118, 270, (270 + angle) % 360, (uint32_t)0xFF9F0A);
    }

    // 2. Title Pill
    _data.hal->canvas->fillRoundRect(65, 12, 110, 22, 11, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawRoundRect(65, 12, 110, 22, 11, (uint32_t)0x333333);
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setTextColor((uint32_t)0xFF9F0A, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawString("STOPWATCH", 120, 23);

    // 3. Time Display
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", m, s);
    char cs_str[8];
    snprintf(cs_str, sizeof(cs_str), ".%02d", c);

    _data.hal->canvas->setTextDatum(textdatum_t::middle_right);
    _data.hal->canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    _data.hal->canvas->setFont(&fonts::Font4);
    _data.hal->canvas->drawString(time_str, 142, 85);

    _data.hal->canvas->setTextDatum(textdatum_t::middle_left);
    _data.hal->canvas->setTextColor((uint32_t)0xFF9F0A, TFT_BLACK);
    _data.hal->canvas->setFont(&fonts::Font2);
    _data.hal->canvas->drawString(cs_str, 144, 88);

    // 4. Lap Details
    _data.hal->canvas->setFont(&fonts::Font0);
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    if (_data.lap_count > 0) {
        char lap_buf[48];
        int last_idx = _data.lap_count - 1;
        int64_t lap_delta = (last_idx > 0) ? (_data.laps[last_idx] - _data.laps[last_idx - 1]) : _data.laps[0];
        char dt_buf[24];
        _format_time(dt_buf, sizeof(dt_buf), lap_delta);
        snprintf(lap_buf, sizeof(lap_buf), "Lap %d  %s", _data.lap_count, dt_buf);
        _data.hal->canvas->setTextColor((uint32_t)0xCCCCCC, TFT_BLACK);
        _data.hal->canvas->drawString(lap_buf, 120, 128);

        if (_data.lap_count > 1) {
            int prev_idx = _data.lap_count - 2;
            int64_t prev_delta = (prev_idx > 0) ? (_data.laps[prev_idx] - _data.laps[prev_idx - 1]) : _data.laps[0];
            char pdt_buf[24];
            _format_time(pdt_buf, sizeof(pdt_buf), prev_delta);
            char prev_lap_buf[48];
            snprintf(prev_lap_buf, sizeof(prev_lap_buf), "Lap %d  %s", _data.lap_count - 1, pdt_buf);
            _data.hal->canvas->setTextColor((uint32_t)0x777777, TFT_BLACK);
            _data.hal->canvas->drawString(prev_lap_buf, 120, 146);
        }
    } else {
        _data.hal->canvas->setTextColor((uint32_t)0x666666, TFT_BLACK);
        _data.hal->canvas->drawString("Click Dial: Start / Stop", 120, 134);
    }

    // 5. Bottom Control Pills
    // Left: Lap (if running) or Reset (if stopped)
    _data.hal->canvas->fillRoundRect(36, 172, 70, 32, 16, (uint32_t)0x2A2A2E);
    _data.hal->canvas->setTextColor(TFT_WHITE, (uint32_t)0x2A2A2E);
    _data.hal->canvas->drawString(_data.running ? "Lap" : "Reset", 71, 188);

    // Right: Start (green) or Pause (red/orange)
    uint32_t btn_col = _data.running ? (uint32_t)0xD32F2F : (uint32_t)0x2E7D32;
    _data.hal->canvas->fillRoundRect(134, 172, 70, 32, 16, btn_col);
    _data.hal->canvas->setTextColor(TFT_WHITE, btn_col);
    _data.hal->canvas->drawString(_data.running ? "Pause" : "Start", 169, 188);

    _data.hal->canvas->pushSprite(0, 0);
}

void StopwatchApp::onSetup()
{
    setAppName("Stopwatch");
    setAllowBgRunning(false);

    STOPWATCH::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void StopwatchApp::onCreate()
{
    _log("onCreate");
    _render();
}

void StopwatchApp::onRunning()
{
    // 1. Encoder Knob Click / Long Press
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
        _start_stop();
    }

    // 2. Rotary Dial rotation: CW = Lap (if running), CCW = Reset (if stopped)
    if (_data.hal->encoder.wasMoved(true))
    {
        if (_data.running && _data.hal->encoder.getDirection() < 1) {
            _lap_reset();
        } else if (!_data.running && _data.hal->encoder.getDirection() >= 1) {
            _lap_reset();
        }
    }

    // 3. Touch Handling
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

        // Tap gestures
        if (abs(dx) <= 25 && abs(dy) <= 25 && duration < 600)
        {
            if (start_y >= 160) {
                if (start_x < 115) {
                    _lap_reset();
                } else if (start_x > 125) {
                    _start_stop();
                }
            } else {
                _start_stop();
            }
        }
    }

    // 4. Update display while running (every ~40ms = 25fps)
    if (_data.running && (millis() - _data.last_render > 40))
    {
        _data.last_render = millis();
        _render();
    }

    delay(10);
}

void StopwatchApp::onDestroy()
{
    _log("onDestroy");
    if (_data.hal != nullptr)
    {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_stopwatch_app(HAL::HAL* hal)
{
    StopwatchApp app;
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
