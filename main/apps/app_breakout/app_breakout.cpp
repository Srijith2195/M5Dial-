/**
 * @file app_breakout.cpp
 * @brief Retro Rotary Breakout / Pong Arcade Game for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#include "app_breakout.h"
#include "../common_define.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdlib>

using namespace MOONCAKE::USER_APP;

void BreakoutApp::_init_game()
{
    _data.score = 0;
    _data.lives = 3;
    _data.paddle_x = 120.0f;
    _data.bricks_left = 30;

    static const uint32_t row_colors[5] = {
        (uint32_t)0xFF1744, // Red
        (uint32_t)0xFF6D00, // Orange
        (uint32_t)0xFFEA00, // Yellow
        (uint32_t)0x00E676, // Green
        (uint32_t)0x00E5FF  // Cyan
    };
    static const int row_points[5] = { 50, 40, 30, 20, 10 };

    int b_w = 26;
    int b_h = 10;
    int start_x = 120 - (6 * (b_w + 2)) / 2;
    int start_y = 38;

    int idx = 0;
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 6; c++) {
            _data.bricks[idx].x = start_x + c * (b_w + 2);
            _data.bricks[idx].y = start_y + r * (b_h + 3);
            _data.bricks[idx].w = b_w;
            _data.bricks[idx].h = b_h;
            _data.bricks[idx].color = row_colors[r];
            _data.bricks[idx].points = row_points[r];
            _data.bricks[idx].alive = true;
            idx++;
        }
    }

    _reset_ball();
    _data.state = BREAKOUT::READY;
}

void BreakoutApp::_reset_ball()
{
    _data.ball_x = _data.paddle_x;
    _data.ball_y = _data.paddle_y - 8;
    _data.ball_vx = (rand() % 2 == 0 ? 2.2f : -2.2f);
    _data.ball_vy = -3.2f;
}

void BreakoutApp::_update_physics()
{
    if (_data.state != BREAKOUT::PLAYING) return;

    _data.ball_x += _data.ball_vx;
    _data.ball_y += _data.ball_vy;

    // Wall collision (circular bounds approximation)
    if (_data.ball_x - _data.ball_r <= 22) {
        _data.ball_x = 22 + _data.ball_r;
        _data.ball_vx = -_data.ball_vx;
        if (_data.hal) _data.hal->buzz.tone(1800, 10);
    } else if (_data.ball_x + _data.ball_r >= 218) {
        _data.ball_x = 218 - _data.ball_r;
        _data.ball_vx = -_data.ball_vx;
        if (_data.hal) _data.hal->buzz.tone(1800, 10);
    }

    if (_data.ball_y - _data.ball_r <= 26) {
        _data.ball_y = 26 + _data.ball_r;
        _data.ball_vy = -_data.ball_vy;
        if (_data.hal) _data.hal->buzz.tone(1800, 10);
    }

    // Paddle collision
    int p_left = (int)(_data.paddle_x - _data.paddle_w / 2);
    int p_right = (int)(_data.paddle_x + _data.paddle_w / 2);
    if (_data.ball_vy > 0 &&
        _data.ball_y + _data.ball_r >= _data.paddle_y &&
        _data.ball_y - _data.ball_r <= _data.paddle_y + _data.paddle_h &&
        _data.ball_x >= p_left - 4 && _data.ball_x <= p_right + 4)
    {
        _data.ball_y = _data.paddle_y - _data.ball_r;
        float hit_norm = (_data.ball_x - _data.paddle_x) / (_data.paddle_w / 2.0f);
        hit_norm = std::max(-1.0f, std::min(1.0f, hit_norm));
        _data.ball_vx = hit_norm * 3.8f;
        _data.ball_vy = -sqrtf(std::max(4.0f, 15.0f - _data.ball_vx * _data.ball_vx));
        if (_data.hal) _data.hal->buzz.tone(1400, 15);
    }

    // Brick collisions
    for (int i = 0; i < 30; i++) {
        if (!_data.bricks[i].alive) continue;
        const auto& b = _data.bricks[i];
        if (_data.ball_x + _data.ball_r >= b.x && _data.ball_x - _data.ball_r <= b.x + b.w &&
            _data.ball_y + _data.ball_r >= b.y && _data.ball_y - _data.ball_r <= b.y + b.h)
        {
            _data.bricks[i].alive = false;
            _data.bricks_left--;
            _data.score += b.points;
            _data.ball_vy = -_data.ball_vy;

            if (_data.hal) _data.hal->buzz.tone(2600 + b.points * 15, 20);

            if (_data.bricks_left <= 0) {
                _data.state = BREAKOUT::VICTORY;
                if (_data.hal) {
                    _data.hal->buzz.tone(3000, 50);
                    delay(70);
                    _data.hal->buzz.tone(4000, 80);
                }
            }
            break;
        }
    }

    // Ball dropped (life lost)
    if (_data.ball_y > 228) {
        _data.lives--;
        if (_data.lives > 0) {
            _data.state = BREAKOUT::LIFE_LOST;
            if (_data.hal) _data.hal->buzz.tone(600, 90);
            _reset_ball();
        } else {
            _data.state = BREAKOUT::GAME_OVER;
            if (_data.hal) {
                _data.hal->buzz.tone(800, 80);
                delay(90);
                _data.hal->buzz.tone(500, 140);
            }
        }
    }
}

void BreakoutApp::_render()
{
    if (_data.hal == nullptr) return;

    _data.hal->canvas->fillScreen(TFT_BLACK);

    // 1. Circular boundary guide
    _data.hal->canvas->drawCircle(120, 120, 118, (uint32_t)0x222222);

    // 2. Score & Lives Bar at top
    _data.hal->canvas->setTextDatum(textdatum_t::middle_left);
    _data.hal->canvas->setTextColor((uint32_t)0xAAAAAA, TFT_BLACK);
    _data.hal->canvas->setFont(&fonts::Font0);

    char score_buf[32];
    snprintf(score_buf, sizeof(score_buf), "%d PTS", _data.score);
    _data.hal->canvas->drawString(score_buf, 55, 18);

    // Lives (green dots)
    _data.hal->canvas->setTextDatum(textdatum_t::middle_right);
    _data.hal->canvas->drawString("LIVES:", 158, 18);
    for (int l = 0; l < 3; l++) {
        uint32_t l_col = (l < _data.lives) ? (uint32_t)0x00E676 : (uint32_t)0x444444;
        _data.hal->canvas->fillCircle(168 + l * 8, 18, 3, l_col);
    }

    // 3. Bricks
    for (int i = 0; i < 30; i++) {
        if (_data.bricks[i].alive) {
            _data.hal->canvas->fillRoundRect(_data.bricks[i].x, _data.bricks[i].y, _data.bricks[i].w, _data.bricks[i].h, 2, _data.bricks[i].color);
        }
    }

    // 4. Paddle
    int p_left = (int)(_data.paddle_x - _data.paddle_w / 2);
    _data.hal->canvas->fillRoundRect(p_left, _data.paddle_y, _data.paddle_w, _data.paddle_h, 3, (uint32_t)0x00E676);
    _data.hal->canvas->drawRoundRect(p_left, _data.paddle_y, _data.paddle_w, _data.paddle_h, 3, TFT_WHITE);

    // 5. Ball
    _data.hal->canvas->fillCircle((int)_data.ball_x, (int)_data.ball_y, _data.ball_r, TFT_WHITE);

    // 6. State Overlay Messages
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    if (_data.state == BREAKOUT::READY) {
        _data.hal->canvas->fillRoundRect(45, 115, 150, 42, 10, (uint32_t)0x1A1A1A);
        _data.hal->canvas->drawRoundRect(45, 115, 150, 42, 10, (uint32_t)0x00E676);
        _data.hal->canvas->setTextColor((uint32_t)0x00E676, (uint32_t)0x1A1A1A);
        _data.hal->canvas->drawString("ROTARY PONG", 120, 128);
        _data.hal->canvas->setTextColor(TFT_WHITE, (uint32_t)0x1A1A1A);
        _data.hal->canvas->drawString("Click or Tap to Serve!", 120, 144);
    } else if (_data.state == BREAKOUT::LIFE_LOST) {
        _data.hal->canvas->fillRoundRect(50, 120, 140, 36, 10, (uint32_t)0x2A1A00);
        _data.hal->canvas->drawRoundRect(50, 120, 140, 36, 10, (uint32_t)0xFF9800);
        _data.hal->canvas->setTextColor((uint32_t)0xFF9800, (uint32_t)0x2A1A00);
        _data.hal->canvas->drawString("BALL LOST", 120, 131);
        _data.hal->canvas->setTextColor(TFT_WHITE, (uint32_t)0x2A1A00);
        _data.hal->canvas->drawString("Click / Tap to Continue", 120, 145);
    } else if (_data.state == BREAKOUT::GAME_OVER) {
        _data.hal->canvas->fillRoundRect(50, 115, 140, 42, 10, (uint32_t)0x2E0B0B);
        _data.hal->canvas->drawRoundRect(50, 115, 140, 42, 10, (uint32_t)0xFF1744);
        _data.hal->canvas->setTextColor((uint32_t)0xFF1744, (uint32_t)0x2E0B0B);
        _data.hal->canvas->drawString("GAME OVER", 120, 128);
        _data.hal->canvas->setTextColor(TFT_WHITE, (uint32_t)0x2E0B0B);
        _data.hal->canvas->drawString("Click / Tap to Restart", 120, 144);
    } else if (_data.state == BREAKOUT::VICTORY) {
        _data.hal->canvas->fillRoundRect(50, 115, 140, 42, 10, (uint32_t)0x0B2E13);
        _data.hal->canvas->drawRoundRect(50, 115, 140, 42, 10, (uint32_t)0x00E676);
        _data.hal->canvas->setTextColor((uint32_t)0x00E676, (uint32_t)0x0B2E13);
        _data.hal->canvas->drawString("YOU WIN!", 120, 128);
        _data.hal->canvas->setTextColor(TFT_WHITE, (uint32_t)0x0B2E13);
        _data.hal->canvas->drawString("Click / Tap to Play Again", 120, 144);
    }

    _data.hal->canvas->pushSprite(0, 0);
}

void BreakoutApp::onSetup()
{
    setAppName("Breakout");
    setAllowBgRunning(false);

    BREAKOUT::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void BreakoutApp::onCreate()
{
    _log("onCreate");
    _init_game();
    _render();
}

void BreakoutApp::onRunning()
{
    // 1. Dial Knob rotation: Move Paddle smoothly
    if (_data.hal->encoder.wasMoved(true))
    {
        int dir = _data.hal->encoder.getDirection();
        float speed = 7.0f;
        if (dir < 1) {
            _data.paddle_x = std::min(195.0f, _data.paddle_x + speed);
        } else {
            _data.paddle_x = std::max(45.0f, _data.paddle_x - speed);
        }

        if (_data.state == BREAKOUT::READY || _data.state == BREAKOUT::LIFE_LOST) {
            _data.ball_x = _data.paddle_x;
        }
    }

    // 2. Encoder Knob Click: Serve / Restart / Exit on long press
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

        if (_data.state == BREAKOUT::READY || _data.state == BREAKOUT::LIFE_LOST) {
            _data.state = BREAKOUT::PLAYING;
            _data.hal->buzz.tone(3400, 30);
        } else if (_data.state == BREAKOUT::GAME_OVER || _data.state == BREAKOUT::VICTORY) {
            _init_game();
        }
    }

    // 3. Touch Screen: Drag paddle or tap to serve; Swipe Left to Exit
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

            // Touch drag paddle horizontally
            if (last_y >= 160) {
                _data.paddle_x = std::max(45.0f, std::min(195.0f, (float)last_x));
                if (_data.state == BREAKOUT::READY || _data.state == BREAKOUT::LIFE_LOST) {
                    _data.ball_x = _data.paddle_x;
                }
            }
            delay(10);
        }

        int dx = last_x - start_x;
        int dy = last_y - start_y;
        uint32_t duration = millis() - start_time;

        // Swipe Left -> Exit
        if (dx < -40 && abs(dy) < 55)
        {
            _data.hal->buzz.tone(3000, 30);
            destroyApp();
            return;
        }

        // Tap / Screen Click (or release after paddle drag in non-playing states)
        if (duration < 600)
        {
            if (_data.state == BREAKOUT::READY || _data.state == BREAKOUT::LIFE_LOST) {
                _data.state = BREAKOUT::PLAYING;
                _data.hal->buzz.tone(3400, 30);
            } else if (_data.state == BREAKOUT::GAME_OVER || _data.state == BREAKOUT::VICTORY) {
                _init_game();
            }
        }
    }

    // 4. Game physics loop (~45 fps when playing)
    if (_data.state == BREAKOUT::PLAYING) {
        _update_physics();
    }

    _render();
    delay((_data.state == BREAKOUT::PLAYING) ? 18 : 12);
}

void BreakoutApp::onDestroy()
{
    _log("onDestroy");
    if (_data.hal != nullptr)
    {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_breakout_app(HAL::HAL* hal)
{
    BreakoutApp app;
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
