/**
 * @file app_vault.cpp
 * @brief Secret Msg App for M5Dial (Pure Pixel Heart, Intuitive Typing, Touch & Dial)
 * @version 1.2
 * @date 2026-11-20
 */
#include "app_vault.h"
#include "../common_define.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <sstream>

using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::VAULT;

static const char* VAULT_CHARS[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z", "0", "1", "2", "3",
    "4", "5", "6", "7", "8", "9", " ", "!", "?", "<", "OK"
};
static const int TOTAL_VAULT_CHARS = 41;

/* ---------------- Lifecycle ---------------- */

void VaultApp::onSetup()
{
    setAppName("Secret Msg");
    _data.hal = (HAL::HAL*)getUserData();
}

void VaultApp::onCreate()
{
    if (_data.hal == nullptr) return;

    // Fresh start every time app opens
    _data.state = VAULT_INPUT;
    _data.state_start_time = millis();
    _data.message.clear();
    _data.char_idx = 0; // Starts at 'A'
    _data.cursor_blink_time = millis();
    _data.cursor_visible = true;
    _data.heart_scale = 4.5f;
    _data.last_beat_time = millis();
    _data.beat_phase = false;
    _data.particles.clear();
    _data.last_ambient_spawn = millis();

    _sound_fanfare();
}

void VaultApp::onRunning()
{
    if (_data.hal == nullptr) return;

    switch (_data.state)
    {
        case VAULT_INPUT:
            _input_update();
            _input_render();
            break;
        case VAULT_BREATHING:
            _breathing_update();
            _breathing_render();
            break;
        case VAULT_BURST:
            _burst_update();
            _burst_render();
            break;
        case VAULT_REVEAL:
            _reveal_update();
            _reveal_render();
            break;
    }

    _data.hal->canvas->pushSprite(0, 0);
}

void VaultApp::onDestroy()
{
    _data.message.clear();
    _data.particles.clear();
}

/* ---------------- 1. Input State ---------------- */

void VaultApp::_input_update()
{
    _input_handle_encoder();
    _input_handle_touch();

    // Cursor blink
    if (millis() - _data.cursor_blink_time > 400)
    {
        _data.cursor_blink_time = millis();
        _data.cursor_visible = !_data.cursor_visible;
    }
}

void VaultApp::_input_handle_encoder()
{
    // Rotary Dial moved: cycle through character wheel
    if (_data.hal->encoder.wasMoved(true))
    {
        _sound_click();
        if (_data.hal->encoder.getDirection() < 1)
        {
            _data.char_idx = (_data.char_idx + 1) % TOTAL_VAULT_CHARS;
        }
        else
        {
            _data.char_idx = (_data.char_idx - 1 + TOTAL_VAULT_CHARS) % TOTAL_VAULT_CHARS;
        }
    }

    // Rotary Dial push button
    if (!_data.hal->encoder.btn.read())
    {
        uint32_t press_t = millis();
        while (!_data.hal->encoder.btn.read())
        {
            delay(10);
            if (millis() - press_t > 900)
            {
                // Long press (>900ms): Exit to launcher
                _sound_click();
                while (!_data.hal->encoder.btn.read()) delay(10);
                destroyApp();
                return;
            }
        }
        // Short click: commit character (or finish if on OK)
        _commit_char();
    }
}

void VaultApp::_input_handle_touch()
{
    if (!_data.hal->tp.isTouched()) return;

    _data.hal->tp.update();
    auto pt = _data.hal->tp.getTouchPointBuffer();
    int tx = pt.x;
    int ty = pt.y;

    // Drain touch release with safety timeout (500ms max)
    uint32_t t0 = millis();
    while (_data.hal->tp.isTouched())
    {
        delay(10);
        if (millis() - t0 > 500) break;
    }

    // 1. Bottom Dedicated Button Bar (ty >= 165)
    if (ty >= 165)
    {
        if (tx >= 148)
        {
            // TAP [ OK ♥ ] BUTTON: Immediately commit message & proceed to breathing heart!
            _data.char_idx = TOTAL_VAULT_CHARS - 1; // "OK"
            _commit_char();
            return;
        }
        else if (tx <= 80)
        {
            // TAP [ < DEL ] BUTTON: Backspace
            if (!_data.message.empty())
            {
                _data.message.pop_back();
                _data.hal->buzz.tone(1800, 25);
            }
            return;
        }
        else
        {
            // TAP [ SPACE ] BUTTON: Space
            if (_data.message.length() < 28)
            {
                _data.message += " ";
                _data.hal->buzz.tone(3000, 20);
            }
            return;
        }
    }

    // 2. Center Character Carousel Row (ty >= 90 && ty < 165)
    if (ty >= 90 && ty < 165)
    {
        if (tx >= 75 && tx <= 165)
        {
            // Tap Center Bubble: ADD CURRENT LETTER TO MESSAGE
            _commit_char();
            return;
        }
        else if (tx < 75)
        {
            // Tap Left Arrow: Previous letter
            _data.char_idx = (_data.char_idx - 1 + TOTAL_VAULT_CHARS) % TOTAL_VAULT_CHARS;
            _sound_click();
            return;
        }
        else if (tx > 165)
        {
            // Tap Right Arrow: Next letter
            _data.char_idx = (_data.char_idx + 1) % TOTAL_VAULT_CHARS;
            _sound_click();
            return;
        }
    }

    // 3. Top Message Box (ty >= 30 && ty < 90): Tap to Backspace
    if (ty >= 30 && ty < 90)
    {
        if (!_data.message.empty())
        {
            _data.message.pop_back();
            _data.hal->buzz.tone(1800, 25);
        }
        return;
    }
}

void VaultApp::_commit_char()
{
    std::string ch = VAULT_CHARS[_data.char_idx];

    if (ch == "<")
    {
        // Backspace
        if (!_data.message.empty())
        {
            _data.message.pop_back();
            _data.hal->buzz.tone(1800, 25);
        }
    }
    else if (ch == "OK")
    {
        // Commit and seal message into heart!
        if (_data.message.empty())
        {
            _data.message = "I LOVE YOU";
        }

        _data.state = VAULT_BREATHING;
        _data.state_start_time = millis();
        _data.last_beat_time = millis();
        _data.heart_scale = 4.5f;
        _data.particles.clear();

        _sound_reveal();
    }
    else
    {
        // Add selected character to message
        if (_data.message.length() < 28)
        {
            _data.message += ch;
            _data.hal->buzz.tone(3600, 25);
        }
        else
        {
            _data.hal->buzz.tone(1200, 50); // Length limit reached
        }
    }
}

void VaultApp::_input_render()
{
    auto* canvas = _data.hal->canvas;
    canvas->fillScreen(TFT_BLACK);
    canvas->setTextSize(1);

    // Top Header Title
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor((uint32_t)0xFF80AB);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("~ SECRET MESSAGE ~", 120, 16);

    // 1. Entered Message Box (Prominent & High-Contrast)
    canvas->fillRoundRect(12, 30, 216, 52, 8, (uint32_t)0x1B0010);
    canvas->drawRoundRect(12, 30, 216, 52, 8, (uint32_t)0xFF1493);
    canvas->drawRoundRect(13, 31, 214, 50, 7, (uint32_t)0x880033);

    if (_data.message.empty())
    {
        // Prompt when empty
        canvas->setTextColor((uint32_t)0x888888);
        canvas->setFont(&fonts::Font0);
        canvas->drawString("TYPE MSG WITH DIAL / TAP", 120, 48);
        canvas->setTextColor((uint32_t)0xFF4081);
        canvas->drawString(_data.cursor_visible ? "_" : " ", 120, 64);
    }
    else
    {
        // Display actual entered letters
        std::string disp = _data.message;
        std::string visible_disp = disp;
        if (disp.length() > 16)
        {
            visible_disp = disp.substr(disp.length() - 16);
        }
        if (_data.cursor_visible)
        {
            visible_disp += "_";
        }

        canvas->setTextColor((uint32_t)0xFFFFFF);
        canvas->setFont(&fonts::Font2);
        canvas->drawString(visible_disp.c_str(), 120, 50);

        // Character counter
        char count_buf[16];
        snprintf(count_buf, sizeof(count_buf), "%d / 28", (int)_data.message.length());
        canvas->setFont(&fonts::Font0);
        canvas->setTextColor((uint32_t)0xFFA726);
        canvas->drawString(count_buf, 120, 68);
    }

    // 2. Character Carousel (Center: y = 126)
    int cx = 120, cy = 126;

    // Guide circles
    canvas->drawCircle(cx, cy, 32, (uint32_t)0x330018);

    // Flank letters (-1 and +1)
    int prev_idx = (_data.char_idx - 1 + TOTAL_VAULT_CHARS) % TOTAL_VAULT_CHARS;
    int next_idx = (_data.char_idx + 1) % TOTAL_VAULT_CHARS;

    canvas->setTextColor((uint32_t)0xAA4060);
    canvas->setFont(&fonts::Font2);
    canvas->drawString(VAULT_CHARS[prev_idx], cx - 52, cy);
    canvas->drawString(VAULT_CHARS[next_idx], cx + 52, cy);

    canvas->setFont(&fonts::Font0);
    canvas->setTextColor((uint32_t)0x661830);
    canvas->drawString("<", cx - 78, cy);
    canvas->drawString(">", cx + 78, cy);

    // Selected Character Bubble (Large, Glowing, Clear)
    canvas->fillSmoothCircle(cx, cy, 25, (uint32_t)0xFF1493);
    canvas->drawCircle(cx, cy, 26, (uint32_t)0xFF80AB);
    canvas->drawCircle(cx, cy, 27, (uint32_t)0xFFFFFF);

    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(TFT_WHITE);
    canvas->setFont(&fonts::Font2);
    canvas->drawString(VAULT_CHARS[_data.char_idx], cx, cy);

    // "+ ADD" hint directly under center bubble
    canvas->setTextColor((uint32_t)0xFFD700);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("+ ADD", cx, cy + 30);

    // 3. Dedicated Bottom Touch Buttons
    // Left: [ < DEL ]
    canvas->fillRoundRect(20, 172, 58, 40, 6, (uint32_t)0x2D0014);
    canvas->drawRoundRect(20, 172, 58, 40, 6, (uint32_t)0xFF5252);
    canvas->setTextColor((uint32_t)0xFF5252);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("< DEL", 49, 192);

    // Center: [ SPACE ]
    canvas->fillRoundRect(84, 172, 64, 40, 6, (uint32_t)0x002028);
    canvas->drawRoundRect(84, 172, 64, 40, 6, (uint32_t)0x4DD0E1);
    canvas->setTextColor((uint32_t)0x80D8FF);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("SPACE", 116, 192);

    // Right: [ OK ♥ ] - Bright Glowing Emerald Green Action Button
    canvas->fillRoundRect(154, 172, 66, 40, 6, (uint32_t)0x00E676);
    canvas->drawRoundRect(154, 172, 66, 40, 6, (uint32_t)0xFFFFFF);
    canvas->drawRoundRect(155, 173, 64, 38, 5, (uint32_t)0x00C853);
    canvas->setTextColor(TFT_BLACK);
    canvas->setFont(&fonts::Font2);
    canvas->drawString("OK", 187, 192);

    // Footer Help Hint
    canvas->setTextColor((uint32_t)0x777777);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("Turn knob: Scroll  |  Tap OK to seal", 120, 226);
}

/* ---------------- 2. Breathing State ---------------- */

void VaultApp::_breathing_update()
{
    uint32_t now = millis();
    uint32_t dt = now - _data.state_start_time;

    // Heartbeat pulse cycle (period = 850 ms)
    uint32_t phase_time = dt % 850;

    if (phase_time < 120)
    {
        // First lub beat
        float p = phase_time / 120.0f;
        _data.heart_scale = 4.3f + sin(p * 3.14159f) * 1.3f;
        if (!_data.beat_phase && phase_time < 30)
        {
            _data.beat_phase = true;
            _sound_heartbeat();
        }
    }
    else if (phase_time < 220)
    {
        // Brief relaxation
        _data.heart_scale = 4.6f;
    }
    else if (phase_time < 380)
    {
        // Second dub beat (stronger pulse)
        float p = (phase_time - 220) / 160.0f;
        _data.heart_scale = 4.6f + sin(p * 3.14159f) * 1.6f;
    }
    else
    {
        // Resting phase
        _data.heart_scale = 4.3f;
        _data.beat_phase = false;
    }

    // Ambient floating soft sparkles/hearts
    if (now - _data.last_ambient_spawn > 260)
    {
        _data.last_ambient_spawn = now;
        _spawn_ambient_heart();
    }
    _update_particles();

    // Check input: User taps the heart to burst!
    _breathing_handle_input();
}

void VaultApp::_breathing_handle_input()
{
    bool triggered = false;

    // 1. Touchscreen interaction: Tap on screen / heart
    if (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        uint32_t t0 = millis();
        while (_data.hal->tp.isTouched() && (millis() - t0 < 500)) delay(10);
        triggered = true;
    }

    // 2. Rotary encoder button pressed
    if (!_data.hal->encoder.btn.read())
    {
        uint32_t t0 = millis();
        while (!_data.hal->encoder.btn.read() && (millis() - t0 < 500)) delay(10);
        triggered = true;
    }

    if (triggered)
    {
        // USER TAPPED THE HEART: BURST INTO PIXELS!
        _data.state = VAULT_BURST;
        _data.burst_start_time = millis();
        _data.burst_scale = _data.heart_scale;
        _spawn_burst_particles();
        _sound_burst();
    }
}

void VaultApp::_breathing_render()
{
    auto* canvas = _data.hal->canvas;
    canvas->fillScreen(TFT_BLACK);
    canvas->setTextSize(1);

    // Subtle pulsating glow ring around dial
    float ring_pulse = 0.5f + 0.5f * sin((millis() - _data.state_start_time) * 0.006f);
    uint32_t ring_color = (ring_pulse > 0.5f) ? 0x660028 : 0x330014;
    canvas->drawCircle(120, 120, 118, ring_color);
    canvas->drawCircle(120, 120, 116, ring_color);

    // Draw ambient floating particles
    _draw_particles();

    // Top Header Banner
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor((uint32_t)0xFF80AB);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("~ SECRET SEALED ~", 120, 22);

    // Center: Breathing Pure Pixel Heart
    _draw_pixel_heart(120, 115, _data.heart_scale);

    // Bottom Callout: Tap Heart to Burst & Reveal!
    uint32_t text_color = ((millis() / 350) % 2 == 0) ? (uint32_t)0xFFFFFF : (uint32_t)0xFF4081;
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(text_color);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("TAP HEART TO BURST!", 120, 204);
    canvas->setTextColor((uint32_t)0x888888);
    canvas->drawString("or press knob button", 120, 220);
}

/* ---------------- 3. Burst State ---------------- */

void VaultApp::_burst_update()
{
    uint32_t now = millis();
    uint32_t dt = now - _data.burst_start_time;
    float p = dt / 1100.0f; // 1.1s total burst duration

    if (p >= 1.0f)
    {
        // Burst finished, heart filled dial face -> REVEAL THE MESSAGE!
        _data.state = VAULT_REVEAL;
        _data.state_start_time = millis();
        _data.particles.clear();
        _sound_reveal();
        return;
    }

    // Heart expands exponentially to fill entire 240x240 screen!
    _data.burst_scale = 4.5f + pow(p, 1.9f) * 35.0f;

    _update_particles();
}

void VaultApp::_burst_render()
{
    auto* canvas = _data.hal->canvas;
    canvas->fillScreen(TFT_BLACK);
    canvas->setTextSize(1);

    // Radial background flash during burst
    uint32_t dt = millis() - _data.burst_start_time;
    if (dt < 300)
    {
        canvas->fillScreen((uint32_t)0x4A0020);
    }
    else if (dt < 600)
    {
        canvas->fillScreen((uint32_t)0x20000C);
    }

    // Draw the rapidly expanding pure heart filling the dial
    _draw_pixel_heart(120, 115, _data.burst_scale);

    // Draw bursting particles
    _draw_particles();
}

/* ---------------- 4. Reveal State ---------------- */

void VaultApp::_reveal_update()
{
    uint32_t now = millis();

    // Ambient floating hearts drifting upwards
    if (now - _data.last_ambient_spawn > 220)
    {
        _data.last_ambient_spawn = now;
        _spawn_ambient_heart();
    }
    _update_particles();

    _reveal_handle_input();
}

void VaultApp::_reveal_handle_input()
{
    // Knob button
    if (!_data.hal->encoder.btn.read())
    {
        uint32_t t0 = millis();
        while (!_data.hal->encoder.btn.read())
        {
            delay(10);
            if (millis() - t0 > 750)
            {
                // Long-press: Exit back to launcher!
                _sound_click();
                while (!_data.hal->encoder.btn.read()) delay(10);
                destroyApp();
                return;
            }
        }
        // Short-click: Exit to launcher or celebrate
        _sound_click();
        destroyApp();
        return;
    }

    // Touch interaction
    if (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        auto pt = _data.hal->tp.getTouchPointBuffer();
        uint32_t t0 = millis();
        while (_data.hal->tp.isTouched() && (millis() - t0 < 500)) delay(10);

        // Tap on screen: Spawn burst of sparkling hearts!
        for (int i = 0; i < 14; i++)
        {
            Particle_t p;
            p.x = pt.x;
            p.y = pt.y;
            float angle = (rand() % 360) * 0.01745f;
            float spd = 1.0f + (rand() % 100) / 30.0f;
            p.vx = cos(angle) * spd;
            p.vy = sin(angle) * spd;
            p.type = rand() % 2;
            p.life = 0;
            p.max_life = 25 + rand() % 20;
            p.color = (rand() % 2 == 0) ? (uint32_t)0xFF1493 : (uint32_t)0xFFD700;
            _data.particles.push_back(p);
        }
        _sound_chime();
    }
}

void VaultApp::_reveal_render()
{
    auto* canvas = _data.hal->canvas;
    canvas->fillScreen((uint32_t)0x16000E); // Deep velvety crimson
    canvas->setTextSize(1);

    // Outer neon glow ring
    canvas->drawCircle(120, 120, 118, (uint32_t)0xFF1493);
    canvas->drawCircle(120, 120, 117, (uint32_t)0xFF4081);

    // Draw ambient floating hearts
    _draw_particles();

    // Top Header
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor((uint32_t)0xFFD700);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("~ SECRET MESSAGE ~", 120, 22);

    // Center Message Card (Elegant Parchment Card with Heart Embellishments)
    int card_x = 20, card_y = 42, card_w = 200, card_h = 142;
    canvas->fillRoundRect(card_x, card_y, card_w, card_h, 12, (uint32_t)0x2D001A);
    canvas->drawRoundRect(card_x, card_y, card_w, card_h, 12, (uint32_t)0xFF4081);
    canvas->drawRoundRect(card_x + 3, card_y + 3, card_w - 6, card_h - 6, 9, (uint32_t)0xFFD700);

    // Draw mini pure heart crest at top of card
    _draw_pixel_heart(120, card_y + 16, 1.4f);

    // Render the secret message with auto-wrap
    _draw_wrapped_text(_data.message, 120, card_y + 80, card_w - 24, (uint32_t)0xFFFFFF);

    // Footer
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor((uint32_t)0xAA7788);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("Tap: Hearts  |  Press: Exit", 120, 204);
    canvas->drawString("Next open asks new msg", 120, 220);
}

/* ---------------- Graphics & FX Helpers ---------------- */

void VaultApp::_draw_pixel_heart(int cx, int cy, float scale)
{
    if (_data.hal == nullptr) return;
    auto* canvas = _data.hal->canvas;

    int iscale = (int)scale;
    if (iscale < 1) iscale = 1;

    int top_left_x = cx - (VAULT::HEART_W * iscale) / 2;
    int top_left_y = cy - (VAULT::HEART_H * iscale) / 2;

    for (int y = 0; y < VAULT::HEART_H; y++)
    {
        for (int x = 0; x < VAULT::HEART_W; x++)
        {
            uint16_t color = VAULT::heart_pixel_sprite[y * VAULT::HEART_W + x];
            if (color != 0x0000)
            {
                // Unswap bytes for LovyanGFX
                uint16_t unswapped = ((color & 0xFF) << 8) | ((color >> 8) & 0xFF);
                canvas->fillRect(top_left_x + x * iscale, top_left_y + y * iscale, iscale, iscale, unswapped);
            }
        }
    }
}

void VaultApp::_draw_wrapped_text(const std::string& text, int cx, int cy, int max_w, uint32_t color)
{
    auto* canvas = _data.hal->canvas;
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(color);
    canvas->setTextSize(1);

    // Choose font size based on text length
    if (text.length() <= 12)
    {
        canvas->setFont(&fonts::Font2);
    }
    else
    {
        canvas->setFont(&fonts::Font0);
    }

    // Split words by space
    std::vector<std::string> words;
    std::stringstream ss(text);
    std::string w;
    while (ss >> w)
    {
        words.push_back(w);
    }

    if (words.empty()) return;

    // Word wrap into lines
    std::vector<std::string> lines;
    std::string cur_line = words[0];

    for (size_t i = 1; i < words.size(); i++)
    {
        std::string test = cur_line + " " + words[i];
        if (canvas->textWidth(test.c_str()) <= max_w)
        {
            cur_line = test;
        }
        else
        {
            lines.push_back(cur_line);
            cur_line = words[i];
        }
    }
    lines.push_back(cur_line);

    // Draw lines vertically centered
    int line_h = (text.length() <= 12) ? 22 : 14;
    int total_h = lines.size() * line_h;
    int start_y = cy - (total_h / 2) + (line_h / 2);

    for (size_t i = 0; i < lines.size(); i++)
    {
        // Drop shadow for crisp readability
        canvas->setTextColor((uint32_t)0x1B000B);
        canvas->drawString(lines[i].c_str(), cx + 1, start_y + (int)i * line_h + 1);

        canvas->setTextColor(color);
        canvas->drawString(lines[i].c_str(), cx, start_y + (int)i * line_h);
    }
}

void VaultApp::_spawn_burst_particles()
{
    // Radial burst of 36 particles
    for (int i = 0; i < 36; i++)
    {
        Particle_t p;
        p.x = 120.0f;
        p.y = 115.0f;
        float angle = (i * 10) * 0.01745f;
        float speed = 2.0f + (rand() % 100) / 25.0f;
        p.vx = cos(angle) * speed;
        p.vy = sin(angle) * speed;
        p.type = rand() % 3; // 0: Mini Heart, 1: Star, 2: Dot
        p.life = 0;
        p.max_life = 30 + rand() % 25;

        uint32_t colors[4] = {0xFF1493, 0xFF4081, 0xFFD700, 0xFFFFFF};
        p.color = colors[rand() % 4];

        _data.particles.push_back(p);
    }
}

void VaultApp::_spawn_ambient_heart()
{
    Particle_t p;
    p.x = 40.0f + (rand() % 160);
    p.y = 230.0f;
    p.vx = ((rand() % 100) - 50) / 80.0f;
    p.vy = -0.8f - (rand() % 100) / 90.0f;
    p.type = 0; // Mini Heart
    p.life = 0;
    p.max_life = 50 + rand() % 30;
    p.color = (rand() % 3 == 0) ? (uint32_t)0xFF80AB : (uint32_t)0xFF1493;

    _data.particles.push_back(p);
}

void VaultApp::_update_particles()
{
    for (auto it = _data.particles.begin(); it != _data.particles.end();)
    {
        it->x += it->vx;
        it->y += it->vy;
        it->life++;

        if (it->life >= it->max_life)
        {
            it = _data.particles.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void VaultApp::_draw_particles()
{
    auto* canvas = _data.hal->canvas;

    for (const auto& p : _data.particles)
    {
        int px = (int)p.x;
        int py = (int)p.y;

        if (p.type == 0) // Mini Heart
        {
            canvas->fillCircle(px - 2, py - 1, 2, p.color);
            canvas->fillCircle(px + 2, py - 1, 2, p.color);
            canvas->fillTriangle(px - 4, py, px + 4, py, px, py + 4, p.color);
        }
        else if (p.type == 1) // Sparkle Star
        {
            canvas->fillCircle(px, py, 1, p.color);
            canvas->drawFastHLine(px - 2, py, 5, p.color);
            canvas->drawFastVLine(px, py - 2, 5, p.color);
        }
        else // Glowing Dot
        {
            canvas->fillCircle(px, py, 2, p.color);
        }
    }
}

/* ---------------- Sound Effects ---------------- */

void VaultApp::_sound_click()
{
    if (_data.hal) _data.hal->buzz.tone(3600, 12);
}

void VaultApp::_sound_heartbeat()
{
    if (_data.hal)
    {
        _data.hal->buzz.tone(140, 25);
    }
}

void VaultApp::_sound_burst()
{
    if (_data.hal)
    {
        _data.hal->buzz.tone(600, 30);
        delay(15);
        _data.hal->buzz.tone(1200, 40);
        delay(15);
        _data.hal->buzz.tone(2400, 70);
    }
}

void VaultApp::_sound_reveal()
{
    if (_data.hal)
    {
        _data.hal->buzz.tone(1800, 40);
        delay(20);
        _data.hal->buzz.tone(2200, 60);
        delay(20);
        _data.hal->buzz.tone(2700, 90);
    }
}

void VaultApp::_sound_chime()
{
    if (_data.hal)
    {
        _data.hal->buzz.tone(2500, 25);
        delay(10);
        _data.hal->buzz.tone(3200, 40);
    }
}

void VaultApp::_sound_fanfare()
{
    if (_data.hal)
    {
        _data.hal->buzz.tone(1500, 30);
        delay(15);
        _data.hal->buzz.tone(2000, 35);
        delay(15);
        _data.hal->buzz.tone(2500, 50);
    }
}
