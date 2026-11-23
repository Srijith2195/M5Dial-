/**
 * @file app_omnitrix.cpp
 * @brief Ben 10 Classic Omnitrix Simulator for M5Dial
 * @version 0.2
 * @date 2026-11-19
 */
#include "app_omnitrix.h"
#include "omnitrix_standby_face.h"
#include "omnitrix_alien_dials.h"
#include "../common_define.h"
#include <cmath>
#include <algorithm>

using namespace MOONCAKE::USER_APP;

void OmnitrixApp::_render_standby()
{
    if (_data.hal == nullptr) return;

    // Display the user-uploaded authentic Omnitrix standby face
    _data.hal->canvas->pushImage(0, 0, 240, 240, image_omnitrix_face);

    // Subtle ambient breathing glow ring
    uint32_t glow_col = _data.pulse_up ? (uint32_t)0x00FF00 : (uint32_t)0x006600;
    _data.hal->canvas->drawCircle(120, 120, 118, glow_col);

    _data.hal->canvas->pushSprite(0, 0);
}

void OmnitrixApp::_render_selection()
{
    if (_data.hal == nullptr) return;

    // Full 240x240 Alien Silhouette Dial Face filling the entire round display
    _data.hal->canvas->pushImage(0, 0, 240, 240, omnitrix_alien_dials[_data.selected_alien]);

    // Zero text, zero indicator dots - pure authentic full-screen Omnitrix dial!
    _data.hal->canvas->pushSprite(0, 0);
}

void OmnitrixApp::_render_transformed()
{
    if (_data.hal == nullptr) return;

    // Display the green Omnitrix dial (like the initial Omnitrix dial)
    _data.hal->canvas->pushImage(0, 0, 240, 240, image_omnitrix_face);

    // Active energized battle perimeter rings
    uint32_t ring_col = _data.pulse_up ? (uint32_t)0x76FF03 : (uint32_t)0x00FF00;
    _data.hal->canvas->drawCircle(120, 120, 119, ring_col);
    _data.hal->canvas->drawCircle(120, 120, 118, ring_col);

    _data.hal->canvas->pushSprite(0, 0);
}

void OmnitrixApp::_render_timeout()
{
    if (_data.hal == nullptr) return;

    // Full authentic red Omnitrix hourglass face during timeout
    _data.hal->canvas->pushImage(0, 0, 240, 240, image_omnitrix_face_red);

    // Flashing outer warning ring during alarm pulses
    uint32_t ring_col = _data.timeout_flash ? (uint32_t)0xFF1744 : (uint32_t)0x660000;
    _data.hal->canvas->drawCircle(120, 120, 119, ring_col);
    _data.hal->canvas->drawCircle(120, 120, 118, ring_col);

    _data.hal->canvas->pushSprite(0, 0);
}

void OmnitrixApp::_popup_dial()
{
    if (_data.hal == nullptr) return;

    // Classic rising Omnitrix activation sound sweep
    _data.hal->buzz.tone(1200, 30); delay(35);
    _data.hal->buzz.tone(1800, 30); delay(35);
    _data.hal->buzz.tone(2600, 40); delay(45);
    _data.hal->buzz.tone(3600, 50); delay(55);
    _data.hal->buzz.tone(4500, 70);

    _data.mode = OMNITRIX::MODE_SELECTION;
    _data.mode_start_time = millis();
    _render_selection();
}

void OmnitrixApp::_slam_dial()
{
    if (_data.hal == nullptr) return;

    // Blinding green transformation screen flash
    _data.hal->canvas->fillScreen((uint32_t)0x00FF00);
    _data.hal->canvas->pushSprite(0, 0);

    // Epic multi-stage transformation sound sequence
    _data.hal->buzz.tone(1000, 40); delay(45);
    _data.hal->buzz.tone(1600, 40); delay(45);
    _data.hal->buzz.tone(2400, 50); delay(55);
    _data.hal->buzz.tone(3600, 80); delay(85);
    _data.hal->buzz.tone(4800, 140);

    _data.mode = OMNITRIX::MODE_TRANSFORMED;
    _data.mode_start_time = millis();
    _render_transformed();
}

void OmnitrixApp::_trigger_timeout()
{
    if (_data.hal == nullptr) return;

    _data.mode = OMNITRIX::MODE_TIMEOUT;
    _data.mode_start_time = millis();
    _data.timeout_beep_time = millis();

    // Red warning timeout alarm
    _data.hal->buzz.tone(1400, 70); delay(80);
    _data.hal->buzz.tone(900, 120);

    _render_timeout();
}

void OmnitrixApp::_reset_standby()
{
    _data.mode = OMNITRIX::MODE_STANDBY;
    _data.mode_start_time = millis();
    if (_data.hal) {
        // Classic Omnitrix recharge ready tone
        _data.hal->buzz.tone(2200, 50);
        delay(60);
        _data.hal->buzz.tone(3200, 80);
    }
    _render_standby();
}

void OmnitrixApp::onSetup()
{
    setAppName("Omnitrix");
    setAllowBgRunning(false);

    OMNITRIX::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void OmnitrixApp::onCreate()
{
    _log("onCreate");
    _data.mode = OMNITRIX::MODE_STANDBY;
    _data.selected_alien = 0;
    _data.mode_start_time = millis();

    if (_data.hal != nullptr) {
        _data.hal->buzz.tone(2400, 40);
    }
    _render_standby();
}

void OmnitrixApp::onRunning()
{
    // 1. Standby Breathing Glow animation
    if (_data.mode == OMNITRIX::MODE_STANDBY)
    {
        if (millis() - _data.last_pulse_time > 800)
        {
            _data.last_pulse_time = millis();
            _data.pulse_up = !_data.pulse_up;
            _render_standby();
        }
    }
    else if (_data.mode == OMNITRIX::MODE_TRANSFORMED)
    {
        if (millis() - _data.last_pulse_time > 500)
        {
            _data.last_pulse_time = millis();
            _data.pulse_up = !_data.pulse_up;
            _render_transformed();
        }
    }

    // 2. Timeout Flashing and Auto-recharge
    if (_data.mode == OMNITRIX::MODE_TIMEOUT)
    {
        if (millis() - _data.timeout_beep_time > 350)
        {
            _data.timeout_beep_time = millis();
            _data.timeout_flash = !_data.timeout_flash;
            _data.hal->buzz.tone(1000, 60);
            _render_timeout();
        }

        // Recharged after 3.5 seconds
        if (millis() - _data.mode_start_time > 3500)
        {
            _reset_standby();
            return;
        }
    }

    // Auto timeout after 25 seconds of transformation
    if (_data.mode == OMNITRIX::MODE_TRANSFORMED)
    {
        if (millis() - _data.mode_start_time > 25000)
        {
            _trigger_timeout();
            return;
        }
    }

    // 3. Dial Rotation: Change Alien (when in Selection Mode)
    if (_data.hal->encoder.wasMoved(true))
    {
        if (_data.mode == OMNITRIX::MODE_SELECTION)
        {
            _data.hal->buzz.tone(3400, 16);
            if (_data.hal->encoder.getDirection() < 1) {
                _data.selected_alien = (_data.selected_alien + 1) % OMNITRIX_ALIEN_COUNT;
            } else {
                _data.selected_alien = (_data.selected_alien - 1 + OMNITRIX_ALIEN_COUNT) % OMNITRIX_ALIEN_COUNT;
            }
            _render_selection();
        }
        else if (_data.mode == OMNITRIX::MODE_STANDBY)
        {
            // Dial turn on standby pops up selection mode!
            _popup_dial();
        }
    }

    // 4. Dial Button Click / Long Press
    bool btn_down = (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0);
    if (btn_down)
    {
        uint32_t press_start = millis();
        while (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0)
        {
            delay(10);
            if (millis() - press_start > 650)
            {
                // Long press -> Exit to Launcher
                _data.hal->buzz.tone(2600, 30);
                while (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0) delay(10);
                destroyApp();
                return;
            }
        }

        if (_data.mode == OMNITRIX::MODE_STANDBY)
        {
            _popup_dial();
        }
        else if (_data.mode == OMNITRIX::MODE_SELECTION)
        {
            _slam_dial();
        }
        else if (_data.mode == OMNITRIX::MODE_TRANSFORMED)
        {
            _trigger_timeout();
        }
        else if (_data.mode == OMNITRIX::MODE_TIMEOUT)
        {
            // Low recharge buzz (Omnitrix is cooling down)
            _data.hal->buzz.tone(500, 60);
        }
    }

    // 5. Touch Screen Handling
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

        // Tap
        if (abs(dx) <= 25 && abs(dy) <= 25 && duration < 600)
        {
            if (_data.mode == OMNITRIX::MODE_STANDBY)
            {
                _popup_dial();
            }
            else if (_data.mode == OMNITRIX::MODE_SELECTION)
            {
                _slam_dial();
            }
            else if (_data.mode == OMNITRIX::MODE_TRANSFORMED)
            {
                _trigger_timeout();
            }
            else if (_data.mode == OMNITRIX::MODE_TIMEOUT)
            {
                // Low recharge buzz (Omnitrix is cooling down)
                _data.hal->buzz.tone(500, 60);
            }
        }
    }

    delay(20);
}

void OmnitrixApp::onDestroy()
{
    _log("onDestroy");
    if (_data.hal != nullptr)
    {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_omnitrix_app(HAL::HAL* hal)
{
    OmnitrixApp app;
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
