/**
 * @file app_virtual_dog.cpp
 * @brief Charming Retro Pixel Art Virtual Dog Pet App for M5Dial
 * @version 1.0
 * @date 2026-11-20
 */
#include "app_virtual_dog.h"
#include "app_virtual_dog_sprites.h"
#include "../common_define.h"
#include "nvs_flash.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdlib>

using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::VIRTUAL_DOG;

static const char* NVS_NAMESPACE = "vdog";

static const char* DIAL_CHARS[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
    "U", "V", "W", "X", "Y", "Z", "0", "1", "2", "3",
    "4", "5", "6", "7", "8", "9", " ", "<", "OK"
};
static const int TOTAL_CHARS = 39;

static const char* ACTION_NAMES[] = {
    "FEED", "PLAY", "PET", "SLEEP", "BATH", "STATS"
};

static const char* ACTION_ICONS[] = {
    "EAT", "BALL", "LOVE", "ZZZ", "BATH", "INFO"
};

/* ---------------- Lifecycle ---------------- */

void VirtualDogApp::onSetup()
{
    setAppName("Virtual Dog");
    _data.hal = (HAL::HAL*)getUserData();
}

void VirtualDogApp::onCreate()
{
    if (_data.hal == nullptr) return;

    _data.mode = MODE_PET;
    _data.anim_state = ANIM_IDLE;
    _data.state_start_time = millis();
    _data.last_tick_time = millis();
    _data.last_idle_anim_time = millis();
    _data.last_save_time = millis();
    _data.idle_frame = 0;
    _data.selected_action = 0;
    _data.particles.clear();
    _data.ball.active = false;
    _data.bone.active = false;
    _data.toast_msg.clear();

    // Default Stats
    _data.stats.name = "";
    _data.stats.hunger = 85;
    _data.stats.happiness = 85;
    _data.stats.energy = 90;
    _data.stats.cleanliness = 90;
    _data.stats.level = 1;
    _data.stats.exp = 0;
    _data.stats.stage = 0;

    _load_stats();

    if (_data.stats.name.empty())
    {
        _naming_init();
    }
    else
    {
        _data.mode = MODE_PET;
        _sound_happy_bark();
        _show_toast("HELLO " + _data.stats.name + "!", 0xFFD700);
    }
}

void VirtualDogApp::onRunning()
{
    if (_data.hal == nullptr) return;

    uint32_t now = millis();

    // Stats decay tick every 12 seconds
    if (_data.mode == MODE_PET && now - _data.last_tick_time > 12000)
    {
        _data.last_tick_time = now;
        if (_data.anim_state != ANIM_SLEEPING)
        {
            if (_data.stats.hunger > 0) _data.stats.hunger = std::max(0, _data.stats.hunger - 1);
            if (_data.stats.cleanliness > 0) _data.stats.cleanliness = std::max(0, _data.stats.cleanliness - 1);
            if (_data.stats.hunger < 30 && _data.stats.happiness > 0)
            {
                _data.stats.happiness = std::max(0, _data.stats.happiness - 1);
            }
        }
        else
        {
            // Sleeping recovers energy
            if (_data.stats.energy < 100)
            {
                _data.stats.energy = std::min(100, _data.stats.energy + 5);
            }
        }
    }

    // Auto-save stats every 30 seconds
    if (now - _data.last_save_time > 30000)
    {
        _data.last_save_time = now;
        if (!_data.stats.name.empty())
        {
            _save_stats();
        }
    }

    // Branch by App Mode
    switch (_data.mode)
    {
        case MODE_NAMING:
            _naming_update();
            _naming_render();
            break;
        case MODE_PET:
            _pet_update();
            _pet_render();
            break;
        case MODE_STATS:
            _stats_update();
            _stats_render();
            break;
    }

    _data.hal->canvas->pushSprite(0, 0);
    delay(15);
}

void VirtualDogApp::onDestroy()
{
    _save_stats();
}

/* ---------------- Storage (NVS) ---------------- */

void VirtualDogApp::_load_stats()
{
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return;

    size_t required_size = 0;
    err = nvs_get_str(handle, "name", nullptr, &required_size);
    if (err == ESP_OK && required_size > 1)
    {
        std::vector<char> buf(required_size);
        nvs_get_str(handle, "name", buf.data(), &required_size);
        _data.stats.name = std::string(buf.data());

        int32_t val = 0;
        if (nvs_get_i32(handle, "hunger", &val) == ESP_OK) _data.stats.hunger = val;
        if (nvs_get_i32(handle, "happy", &val) == ESP_OK) _data.stats.happiness = val;
        if (nvs_get_i32(handle, "energy", &val) == ESP_OK) _data.stats.energy = val;
        if (nvs_get_i32(handle, "clean", &val) == ESP_OK) _data.stats.cleanliness = val;
        if (nvs_get_i32(handle, "level", &val) == ESP_OK) _data.stats.level = val;
        if (nvs_get_i32(handle, "exp", &val) == ESP_OK) _data.stats.exp = val;
        if (nvs_get_i32(handle, "stage", &val) == ESP_OK) _data.stats.stage = val;
    }

    nvs_close(handle);
}

void VirtualDogApp::_save_stats()
{
    if (_data.stats.name.empty()) return;

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return;

    nvs_set_str(handle, "name", _data.stats.name.c_str());
    nvs_set_i32(handle, "hunger", _data.stats.hunger);
    nvs_set_i32(handle, "happy", _data.stats.happiness);
    nvs_set_i32(handle, "energy", _data.stats.energy);
    nvs_set_i32(handle, "clean", _data.stats.cleanliness);
    nvs_set_i32(handle, "level", _data.stats.level);
    nvs_set_i32(handle, "exp", _data.stats.exp);
    nvs_set_i32(handle, "stage", _data.stats.stage);

    nvs_commit(handle);
    nvs_close(handle);
}

/* ---------------- Naming Mode (Dial-Picker) ---------------- */

void VirtualDogApp::_naming_init()
{
    _data.mode = MODE_NAMING;
    _data.input_name = "";
    _data.name_char_idx = 0;
    _data.cursor_blink_time = millis();
    _data.cursor_visible = true;
    _sound_happy_bark();
}

void VirtualDogApp::_naming_update()
{
    _naming_handle_encoder();
    _naming_handle_touch();

    if (millis() - _data.cursor_blink_time > 450)
    {
        _data.cursor_blink_time = millis();
        _data.cursor_visible = !_data.cursor_visible;
    }
}

void VirtualDogApp::_naming_handle_encoder()
{
    // Dial Knob moved: Cycle character
    if (_data.hal->encoder.wasMoved(true))
    {
        _sound_click();
        if (_data.hal->encoder.getDirection() < 1)
        {
            _data.name_char_idx = (_data.name_char_idx + 1) % TOTAL_CHARS;
        }
        else
        {
            _data.name_char_idx = (_data.name_char_idx - 1 + TOTAL_CHARS) % TOTAL_CHARS;
        }
    }

    // Dial Knob button pressed: Commit character / Finish
    if (!_data.hal->encoder.btn.read())
    {
        uint32_t press_t = millis();
        while (!_data.hal->encoder.btn.read())
        {
            delay(10);
            if (millis() - press_t > 900)
            {
                // Long press: Exit to launcher if already named
                if (!_data.stats.name.empty())
                {
                    _sound_click();
                    while (!_data.hal->encoder.btn.read()) delay(10);
                    destroyApp();
                    return;
                }
            }
        }
        _naming_commit_char();
    }
}

void VirtualDogApp::_naming_commit_char()
{
    std::string ch = DIAL_CHARS[_data.name_char_idx];
    if (ch == "<")
    {
        if (!_data.input_name.empty())
        {
            _data.input_name.pop_back();
            _data.hal->buzz.tone(1800, 25);
        }
    }
    else if (ch == "OK")
    {
        if (_data.input_name.empty())
        {
            _data.input_name = "BUDDY";
        }
        _data.stats.name = _data.input_name;
        _save_stats();
        _sound_fanfare();
        _show_toast("WELCOME " + _data.stats.name + "!", 0x00FF7F);
        _data.mode = MODE_PET;
        _data.anim_state = ANIM_HAPPY;
        _data.state_start_time = millis();
    }
    else
    {
        if (_data.input_name.length() < 8)
        {
            _data.input_name += ch;
            _data.hal->buzz.tone(3600, 20);
        }
        else
        {
            _data.hal->buzz.tone(1500, 30);
        }
    }
}

void VirtualDogApp::_naming_handle_touch()
{
    if (!_data.hal->tp.isTouched()) return;

    _data.hal->tp.update();
    auto pt = _data.hal->tp.getTouchPointBuffer();
    int s_x = pt.x, s_y = pt.y, l_x = pt.x, l_y = pt.y;
    uint32_t t0 = millis();

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        l_x = _data.hal->tp.getTouchPointBuffer().x;
        l_y = _data.hal->tp.getTouchPointBuffer().y;
        delay(10);
    }

    int dx = l_x - s_x;
    int dy = l_y - s_y;
    uint32_t dt = millis() - t0;

    // Swipe left/right rotates character dial
    if (dx < -35 && abs(dy) < 50)
    {
        _sound_click();
        _data.name_char_idx = (_data.name_char_idx + 1) % TOTAL_CHARS;
        return;
    }
    else if (dx > 35 && abs(dy) < 50)
    {
        _sound_click();
        _data.name_char_idx = (_data.name_char_idx - 1 + TOTAL_CHARS) % TOTAL_CHARS;
        return;
    }

    // Tap detection
    if (abs(dx) <= 20 && abs(dy) <= 20 && dt < 500)
    {
        // Tap on Center Picked Letter (y: 130..190)
        if (s_y >= 130 && s_y <= 190 && s_x >= 70 && s_x <= 170)
        {
            _naming_commit_char();
            return;
        }

        // Tap on Left side (< backspace shortcut or previous char)
        if (s_x < 65 && s_y >= 130 && s_y <= 190)
        {
            _sound_click();
            _data.name_char_idx = (_data.name_char_idx - 1 + TOTAL_CHARS) % TOTAL_CHARS;
            return;
        }

        // Tap on Right side (next char)
        if (s_x > 175 && s_y >= 130 && s_y <= 190)
        {
            _sound_click();
            _data.name_char_idx = (_data.name_char_idx + 1) % TOTAL_CHARS;
            return;
        }

        // Tap on "OK" button shortcut at bottom
        if (s_y >= 195)
        {
            _data.name_char_idx = TOTAL_CHARS - 1; // "OK"
            _naming_commit_char();
            return;
        }
    }
}

void VirtualDogApp::_naming_render()
{
    auto* canvas = _data.hal->canvas;
    canvas->fillScreen(TFT_BLACK);

    // Decorative retro header banner
    canvas->fillRoundRect(35, 12, 170, 24, 12, (uint32_t)0x2E1A0A);
    canvas->drawRoundRect(35, 12, 170, 24, 12, (uint32_t)0xFFA726);
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor((uint32_t)0xFFD54F);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("NAME YOUR PUPPY!", 120, 24);

    // Little preview dog face
    _draw_pixel_sprite(120, 56, VIRTUAL_DOG::sprite_idle_1, 2, 0);

    // Current Name Display Box
    canvas->fillRoundRect(30, 78, 180, 34, 8, (uint32_t)0x1A2228);
    canvas->drawRoundRect(30, 78, 180, 34, 8, (uint32_t)0x4DD0E1);

    std::string display_name = _data.input_name;
    if (_data.cursor_visible && display_name.length() < 8)
    {
        display_name += "_";
    }

    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor((uint32_t)0xE0F7FA);
    canvas->setFont(&fonts::Font2);
    canvas->drawString(display_name.empty() ? "_" : display_name.c_str(), 120, 95);

    // Rotary Dial-Picker Wheel (Wheel Arc / Carousel)
    int cx = 120, cy = 155;

    // Outer wheel guide arc
    canvas->drawCircle(cx, cy, 34, (uint32_t)0x333333);

    // Selected Character highlighted center
    canvas->fillSmoothCircle(cx, cy, 26, (uint32_t)0xFFA726);
    canvas->drawCircle(cx, cy, 27, (uint32_t)0xFFD54F);

    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(TFT_BLACK);
    canvas->setFont(&fonts::Font2);
    canvas->drawString(DIAL_CHARS[_data.name_char_idx], cx, cy);

    // Flanking letters (-2, -1, +1, +2)
    int offsets[4] = {-2, -1, 1, 2};
    int x_shifts[4] = {-65, -34, 34, 65};
    uint32_t flank_colors[4] = {0x666666, 0xAAAAAA, 0xAAAAAA, 0x666666};

    for (int i = 0; i < 4; i++)
    {
        int idx = (_data.name_char_idx + offsets[i] + TOTAL_CHARS * 2) % TOTAL_CHARS;
        canvas->setTextColor(flank_colors[i]);
        canvas->setFont(&fonts::Font0);
        canvas->drawString(DIAL_CHARS[idx], cx + x_shifts[i], cy);
    }

    // Instructions footer
    canvas->setTextColor((uint32_t)0x888888);
    canvas->setFont(&fonts::Font0);
    canvas->drawString("Rotate: Dial  |  Click: Enter", 120, 204);
    canvas->drawString("Select 'OK' When Ready", 120, 222);
}

/* ---------------- Pet Mode ---------------- */

void VirtualDogApp::_pet_update()
{
    _pet_handle_encoder();
    _pet_handle_touch();

    uint32_t now = millis();

    // Idle animation cycle (tail wagging & breathing)
    if (_data.anim_state == ANIM_IDLE)
    {
        if (now - _data.last_idle_anim_time > 400)
        {
            _data.last_idle_anim_time = now;
            _data.idle_frame = (_data.idle_frame + 1) % 2;
        }
    }
    else if (_data.anim_state == ANIM_HAPPY)
    {
        if (now - _data.state_start_time > 2200)
        {
            _data.anim_state = ANIM_IDLE;
        }
    }
    else if (_data.anim_state == ANIM_EATING)
    {
        if (_data.bone.active)
        {
            if (!_data.bone.caught)
            {
                _data.bone.y += _data.bone.vy;
                if (_data.bone.y >= 114)
                {
                    _data.bone.y = 114;
                    _data.bone.caught = true;
                    _data.bone.catch_time = now;
                    _sound_eat();
                    _gain_exp(12);
                    _data.stats.hunger = std::min(100, _data.stats.hunger + 30);
                    _show_toast("NOM NOM NOM!", 0x76FF03);
                    for (int i = 0; i < 6; i++)
                    {
                        _spawn_particle(3, 110 + rand() % 20, 112 + rand() % 8);
                    }
                }
            }
            else
            {
                if (now - _data.bone.catch_time > 2000)
                {
                    _data.bone.active = false;
                    _data.anim_state = ANIM_HAPPY;
                    _data.state_start_time = now;
                    _sound_happy_bark();
                    _show_toast("YUMMY BONE!", 0x00FF7F);
                }
            }
        }
        else
        {
            if (now - _data.state_start_time > 2000)
            {
                _data.anim_state = ANIM_IDLE;
            }
        }
    }
    else if (_data.anim_state == ANIM_PLAYING)
    {
        // Update bouncing ball
        if (_data.ball.active)
        {
            _data.ball.x += _data.ball.vx;
            _data.ball.y += _data.ball.vy;
            _data.ball.vy += 0.45f; // Gravity

            // Bounce off bottom floor
            if (_data.ball.y >= 142)
            {
                _data.ball.y = 142;
                _data.ball.vy = -_data.ball.vy * 0.78f;
                _sound_bounce();
            }

            // Bounce off sides
            if (_data.ball.x <= 40 || _data.ball.x >= 200)
            {
                _data.ball.vx = -_data.ball.vx;
                _sound_bounce();
            }
        }

        if (now - _data.state_start_time > 3000)
        {
            _data.ball.active = false;
            _data.anim_state = ANIM_IDLE;
        }
    }
    else if (_data.anim_state == ANIM_BATH)
    {
        if (now - _data.state_start_time > 2500)
        {
            _data.anim_state = ANIM_IDLE;
        }
    }

    _update_particles();
}

void VirtualDogApp::_pet_handle_encoder()
{
    // Dial Knob moved: Scroll action carousel
    if (_data.hal->encoder.wasMoved(true))
    {
        _sound_click();
        if (_data.hal->encoder.getDirection() < 1)
        {
            _data.selected_action = (_data.selected_action + 1) % ACTION_COUNT;
        }
        else
        {
            _data.selected_action = (_data.selected_action - 1 + ACTION_COUNT) % ACTION_COUNT;
        }
    }

    // Dial Knob button pressed: Execute action or Exit on long-press
    if (!_data.hal->encoder.btn.read())
    {
        uint32_t press_t = millis();
        while (!_data.hal->encoder.btn.read())
        {
            delay(10);
            if (millis() - press_t > 850)
            {
                // Long press: Exit to launcher!
                _sound_click();
                while (!_data.hal->encoder.btn.read()) delay(10);
                destroyApp();
                return;
            }
        }
        // Short click: Execute selected action!
        _trigger_action((PetAction_t)_data.selected_action);
    }
}

void VirtualDogApp::_pet_handle_touch()
{
    if (!_data.hal->tp.isTouched()) return;

    _data.hal->tp.update();
    auto pt = _data.hal->tp.getTouchPointBuffer();
    int s_x = pt.x, s_y = pt.y, l_x = pt.x, l_y = pt.y;
    uint32_t t0 = millis();

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        l_x = _data.hal->tp.getTouchPointBuffer().x;
        l_y = _data.hal->tp.getTouchPointBuffer().y;
        delay(10);
    }

    int dx = l_x - s_x;
    int dy = l_y - s_y;
    uint32_t dt = millis() - t0;

    // Swipe Left to Exit to Launcher
    if (dx < -50 && abs(dy) < 55)
    {
        _sound_click();
        destroyApp();
        return;
    }

    // Swipe Right to toggle Sleep / Wake
    if (dx > 50 && abs(dy) < 55)
    {
        _trigger_action(ACTION_SLEEP);
        return;
    }

    // Tap detection
    if (abs(dx) <= 25 && abs(dy) <= 25 && dt < 500)
    {
        // 1. Direct Tap on Puppy: PET THE PUPPY!
        if (s_y >= 65 && s_y <= 165 && s_x >= 65 && s_x <= 175)
        {
            _trigger_action(ACTION_PET);
            return;
        }

        // 2. Direct Tap on Action Carousel items at bottom (y >= 180)
        if (s_y >= 180)
        {
            // Divide bottom into 6 regions or select clicked action
            int tapped_action = (s_x * ACTION_COUNT) / 240;
            if (tapped_action >= 0 && tapped_action < ACTION_COUNT)
            {
                _data.selected_action = tapped_action;
                _trigger_action((PetAction_t)tapped_action);
            }
            return;
        }

        // 3. Tap on top header (y < 45): open stats
        if (s_y < 45)
        {
            _data.mode = MODE_STATS;
            _sound_click();
            return;
        }
    }
}

void VirtualDogApp::_trigger_action(PetAction_t action)
{
    // If sleeping and user triggers action other than sleep, wake up first
    if (_data.anim_state == ANIM_SLEEPING && action != ACTION_SLEEP)
    {
        _data.anim_state = ANIM_IDLE;
        _sound_happy_bark();
        _show_toast("AWAKE!", 0xFFFF00);
        return;
    }

    switch (action)
    {
        case ACTION_FEED:
            if (_data.stats.hunger >= 100)
            {
                _show_toast("FULL!", 0x4DD0E1);
                _sound_purr();
            }
            else
            {
                _data.anim_state = ANIM_EATING;
                _data.state_start_time = millis();
                _data.bone.active = true;
                _data.bone.x = 120;
                _data.bone.y = 20;
                _data.bone.vy = 2.4f;
                _data.bone.caught = false;
                _data.bone.catch_time = 0;
                _sound_bounce();
                _show_toast("GIVING BONE!", 0xFFD54F);
            }
            break;

        case ACTION_PLAY:
            if (_data.stats.energy < 15)
            {
                _show_toast("TOO TIRED!", 0xFF5252);
                _sound_purr();
            }
            else
            {
                _data.stats.happiness = std::min(100, _data.stats.happiness + 25);
                _data.stats.energy = std::max(0, _data.stats.energy - 15);
                _data.anim_state = ANIM_PLAYING;
                _data.state_start_time = millis();

                // Launch tennis ball
                _data.ball.active = true;
                _data.ball.x = 45;
                _data.ball.y = 120;
                _data.ball.vx = 4.2f;
                _data.ball.vy = -6.5f;

                _sound_bounce();
                _gain_exp(18);
                _show_toast("FETCH!", 0xFFFF00);
            }
            break;

        case ACTION_PET:
            _data.stats.happiness = std::min(100, _data.stats.happiness + 15);
            _data.anim_state = ANIM_HAPPY;
            _data.state_start_time = millis();

            // Spawn cute floating hearts!
            for (int i = 0; i < 4; i++)
            {
                _spawn_particle(0, 100 + rand() % 40, 95 + rand() % 25);
            }

            _sound_happy_bark();
            _gain_exp(8);
            _show_toast("GOOD BOY!", 0xFF4081);
            break;

        case ACTION_SLEEP:
            if (_data.anim_state == ANIM_SLEEPING)
            {
                _data.anim_state = ANIM_IDLE;
                _sound_happy_bark();
                _show_toast("WOKE UP!", 0xFFD54F);
            }
            else
            {
                _data.anim_state = ANIM_SLEEPING;
                _data.state_start_time = millis();
                _spawn_particle(2, 135, 95);
                _sound_purr();
                _show_toast("ZZZ... SLEEPING", 0x80D8FF);
            }
            break;

        case ACTION_BATH:
            _data.stats.cleanliness = 100;
            _data.stats.happiness = std::min(100, _data.stats.happiness + 10);
            _data.anim_state = ANIM_BATH;
            _data.state_start_time = millis();

            // Spawn soap bubbles!
            for (int i = 0; i < 6; i++)
            {
                _spawn_particle(1, 80 + rand() % 80, 100 + rand() % 35);
            }

            _sound_pop();
            _gain_exp(10);
            _show_toast("SQUEAKY CLEAN!", 0x40C4FF);
            break;

        case ACTION_STATS:
            _data.mode = MODE_STATS;
            _sound_click();
            break;

        default:
            break;
    }
}

void VirtualDogApp::_gain_exp(int amount)
{
    _data.stats.exp += amount;
    if (_data.stats.exp >= 100)
    {
        _data.stats.exp -= 100;
        _data.stats.level++;
        _sound_fanfare();
        _show_toast("LEVEL UP! LV." + std::to_string(_data.stats.level), 0xFFD700);
        for (int i = 0; i < 8; i++)
        {
            _spawn_particle(3, 80 + rand() % 80, 70 + rand() % 50);
        }
        _save_stats();
    }
}

void VirtualDogApp::_pet_render()
{
    auto* canvas = _data.hal->canvas;

    // Background color: cozy warm dark slate
    if (_data.anim_state == ANIM_SLEEPING)
    {
        canvas->fillScreen((uint32_t)0x080E18); // Night mode
    }
    else
    {
        canvas->fillScreen((uint32_t)0x141A1E);
    }

    // Floor horizon line
    int floor_y = 145;
    uint32_t floor_color = (_data.anim_state == ANIM_SLEEPING) ? 0x05080E : 0x0F1316;
    canvas->fillRect(0, floor_y, 240, 240 - floor_y, floor_color);
    canvas->drawFastHLine(20, floor_y, 200, (uint32_t)0x263238);

    // Cute Soft Carpet rug under dog
    canvas->fillEllipse(120, 146, 68, 16, (uint32_t)0x1E2930);

    // 1. Top Header Bar
    canvas->setTextDatum(textdatum_t::middle_left);
    canvas->setTextColor((uint32_t)0xFFA726);
    canvas->setFont(&fonts::Font0);

    // Name & Level Pill
    canvas->fillRoundRect(24, 10, 100, 22, 11, (uint32_t)0x232E35);
    canvas->drawRoundRect(24, 10, 100, 22, 11, (uint32_t)0x37474F);
    canvas->drawString(_data.stats.name.c_str(), 36, 21);

    canvas->setTextDatum(textdatum_t::middle_right);
    canvas->setTextColor((uint32_t)0xFFD54F);
    canvas->drawString(("Lv." + std::to_string(_data.stats.level)).c_str(), 116, 21);

    // Mini Status indicators on top right (Hunger 🍖 & Happy 💖)
    // Hunger Bar
    canvas->drawRoundRect(134, 12, 42, 8, 4, (uint32_t)0x555555);
    int h_w = (_data.stats.hunger * 38) / 100;
    if (h_w > 0) canvas->fillRoundRect(136, 14, h_w, 4, 2, (uint32_t)0x76FF03);

    // Happy Bar
    canvas->drawRoundRect(184, 12, 42, 8, 4, (uint32_t)0x555555);
    int p_w = (_data.stats.happiness * 38) / 100;
    if (p_w > 0) canvas->fillRoundRect(186, 14, p_w, 4, 2, (uint32_t)0xFF4081);

    // 2. Pixel Puppy Sprite Rendering (First beloved retro puppy!)
    int dog_x = 120;
    int dog_y = 110;
    int bob_y = 0;
    const uint16_t* current_sprite = VIRTUAL_DOG::sprite_idle_1;

    switch (_data.anim_state)
    {
        case ANIM_IDLE:
            current_sprite = (_data.idle_frame == 0) ? VIRTUAL_DOG::sprite_idle_1 : VIRTUAL_DOG::sprite_idle_2;
            bob_y = (_data.idle_frame == 0) ? 0 : 2;
            break;
        case ANIM_HAPPY: // Love / Petting
        {
            current_sprite = VIRTUAL_DOG::sprite_happy;
            float t = (millis() - _data.state_start_time) * 0.015f;
            bob_y = -abs((int)(sin(t) * 14.0f));
            break;
        }
        case ANIM_EATING: // Feed with bone
        {
            if (_data.bone.active && !_data.bone.caught)
            {
                // Puppy looking up excitedly for the falling bone!
                current_sprite = VIRTUAL_DOG::sprite_happy;
                bob_y = ((millis() / 150) % 2) * 2;
            }
            else
            {
                // Puppy happily chewing the bone
                current_sprite = VIRTUAL_DOG::sprite_eating;
                bob_y = ((millis() / 150) % 2) * 2;
                _draw_pixel_bone(120, 116 + bob_y, 0);
            }
            break;
        }
        case ANIM_PLAYING: // Play Fetch
        {
            current_sprite = VIRTUAL_DOG::sprite_happy;
            if (_data.ball.active)
            {
                dog_x = 120 + (_data.ball.x - 120) * 0.35f;
            }
            float t = (millis() - _data.state_start_time) * 0.018f;
            bob_y = -abs((int)(sin(t) * 16.0f));
            break;
        }
        case ANIM_SLEEPING: // Sleeping with Zzz
        {
            current_sprite = VIRTUAL_DOG::sprite_sleeping;
            bob_y = ((millis() / 600) % 2) * 1;
            if (rand() % 40 == 0)
            {
                _spawn_particle(2, 136, 92);
            }
            break;
        }
        case ANIM_BATH: // Bath with bubbles
        {
            current_sprite = VIRTUAL_DOG::sprite_bath;
            bob_y = ((millis() / 200) % 2) * 2;
            if (rand() % 15 == 0)
            {
                _spawn_particle(1, 85 + rand() % 70, 110 + rand() % 25);
            }
            break;
        }
    }

    // Draw the 16x16 Pixel Dog at scale 5 (80x80 pixels)
    _draw_pixel_sprite(dog_x, dog_y + bob_y, current_sprite, 5, 0);

    // Draw Tennis Ball if playing
    if (_data.ball.active)
    {
        canvas->fillSmoothCircle((int)_data.ball.x, (int)_data.ball.y, 6, (uint32_t)0xCCFF00);
        canvas->drawCircle((int)_data.ball.x, (int)_data.ball.y, 6, (uint32_t)0x76FF03);
    }

    // Draw falling bone if in the air
    if (_data.bone.active && !_data.bone.caught)
    {
        _draw_pixel_bone((int)_data.bone.x, (int)_data.bone.y, 0);
    }

    // 3. Draw Active Particles
    _draw_particles();

    // 4. Draw Action Carousel at bottom
    int icon_cx = 120;
    int carousel_y = 208;

    // Draw carousel item pill
    canvas->fillRoundRect(36, 188, 168, 38, 19, (uint32_t)0x1E2930);
    canvas->drawRoundRect(36, 188, 168, 38, 19, (uint32_t)0x37474F);

    // Selected Action Highlight
    canvas->fillSmoothCircle(icon_cx, carousel_y, 16, (uint32_t)0xFFA726);

    // Selected Action Text
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(TFT_BLACK);
    canvas->setFont(&fonts::Font0);
    canvas->drawString(ACTION_ICONS[_data.selected_action], icon_cx, carousel_y);

    // Flanking Actions (-1 and +1)
    int prev_act = (_data.selected_action - 1 + ACTION_COUNT) % ACTION_COUNT;
    int next_act = (_data.selected_action + 1) % ACTION_COUNT;

    canvas->fillSmoothCircle(icon_cx - 48, carousel_y, 11, (uint32_t)0x263238);
    canvas->setTextColor((uint32_t)0x90A4AE);
    canvas->drawString(ACTION_ICONS[prev_act], icon_cx - 48, carousel_y);

    canvas->fillSmoothCircle(icon_cx + 48, carousel_y, 11, (uint32_t)0x263238);
    canvas->setTextColor((uint32_t)0x90A4AE);
    canvas->drawString(ACTION_ICONS[next_act], icon_cx + 48, carousel_y);

    // Action Name tag underneath
    canvas->setTextColor((uint32_t)0xFFD54F);
    canvas->drawString(ACTION_NAMES[_data.selected_action], 120, 182);

    // Draw Active Toast if any
    _draw_toast();
}

/* ---------------- Stats Mode ---------------- */

void VirtualDogApp::_stats_update()
{
    _stats_handle_encoder();
    _stats_handle_touch();
}

void VirtualDogApp::_stats_handle_encoder()
{
    // Any move or click returns to Pet mode
    if (_data.hal->encoder.wasMoved(true))
    {
        _sound_click();
    }

    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read()) delay(10);
        _sound_click();
        _data.mode = MODE_PET;
    }
}

void VirtualDogApp::_stats_handle_touch()
{
    if (!_data.hal->tp.isTouched()) return;

    _data.hal->tp.update();
    auto pt = _data.hal->tp.getTouchPointBuffer();
    int s_y = pt.y;

    while (_data.hal->tp.isTouched()) delay(10);

    // Tap on Rename button (y >= 195)
    if (s_y >= 195)
    {
        _sound_click();
        _naming_init();
        return;
    }

    // Tap anywhere else: return to Pet Mode
    _sound_click();
    _data.mode = MODE_PET;
}

void VirtualDogApp::_stats_render()
{
    auto* canvas = _data.hal->canvas;
    canvas->fillScreen((uint32_t)0x101418);

    // Header
    canvas->fillRoundRect(40, 10, 160, 24, 12, (uint32_t)0x1E2930);
    canvas->drawRoundRect(40, 10, 160, 24, 12, (uint32_t)0xFFA726);
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor((uint32_t)0xFFD54F);
    canvas->setFont(&fonts::Font0);
    canvas->drawString((_data.stats.name + " STATS").c_str(), 120, 22);

    // EXP Bar & Level
    canvas->setTextDatum(textdatum_t::middle_left);
    canvas->setTextColor((uint32_t)0x90A4AE);
    canvas->drawString(("LEVEL " + std::to_string(_data.stats.level)).c_str(), 35, 48);

    canvas->setTextDatum(textdatum_t::middle_right);
    canvas->setTextColor((uint32_t)0xFFD54F);
    canvas->drawString((std::to_string(_data.stats.exp) + "% EXP").c_str(), 205, 48);

    canvas->fillRoundRect(35, 58, 170, 6, 3, (uint32_t)0x263238);
    int exp_w = (_data.stats.exp * 170) / 100;
    if (exp_w > 0) canvas->fillRoundRect(35, 58, exp_w, 6, 3, (uint32_t)0xFFD54F);

    // Stat Bars: Hunger, Happy, Energy, Clean
    const char* stat_labels[] = {"HUNGER", "HAPPINESS", "ENERGY", "CLEAN"};
    int stat_vals[] = {_data.stats.hunger, _data.stats.happiness, _data.stats.energy, _data.stats.cleanliness};
    uint32_t stat_colors[] = {0x76FF03, 0xFF4081, 0x00E5FF, 0xB388FF};

    int bar_y = 78;
    for (int i = 0; i < 4; i++)
    {
        canvas->setTextDatum(textdatum_t::middle_left);
        canvas->setTextColor((uint32_t)0xCFD8DC);
        canvas->setFont(&fonts::Font0);
        canvas->drawString(stat_labels[i], 35, bar_y);

        canvas->setTextDatum(textdatum_t::middle_right);
        canvas->setTextColor(stat_colors[i]);
        canvas->drawString((std::to_string(stat_vals[i]) + "%").c_str(), 205, bar_y);

        canvas->fillRoundRect(35, bar_y + 10, 170, 7, 3, (uint32_t)0x263238);
        int w = (stat_vals[i] * 170) / 100;
        if (w > 0) canvas->fillRoundRect(35, bar_y + 10, w, 7, 3, stat_colors[i]);

        bar_y += 26;
    }

    // Rename Button Pill at Bottom
    canvas->fillRoundRect(48, 192, 144, 26, 13, (uint32_t)0x263238);
    canvas->drawRoundRect(48, 192, 144, 26, 13, (uint32_t)0x4DD0E1);
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor((uint32_t)0x80D8FF);
    canvas->drawString("TAP TO RENAME", 120, 205);
}

/* ---------------- Pixel Sprite & Particles ---------------- */

void VirtualDogApp::_draw_pixel_sprite(int cx, int cy, const uint16_t* sprite, int scale, int bob_y)
{
    if (sprite == nullptr || _data.hal == nullptr) return;

    int top_left_x = cx - (VIRTUAL_DOG::SPRITE_W * scale) / 2;
    int top_left_y = cy - (VIRTUAL_DOG::SPRITE_H * scale) / 2 + bob_y;

    auto* canvas = _data.hal->canvas;

    for (int y = 0; y < VIRTUAL_DOG::SPRITE_H; y++)
    {
        for (int x = 0; x < VIRTUAL_DOG::SPRITE_W; x++)
        {
            uint16_t color = sprite[y * VIRTUAL_DOG::SPRITE_W + x];
            if (color != 0x0000)
            {
                // Unswap bytes for LovyanGFX fillRect API
                uint16_t unswapped = ((color & 0xFF) << 8) | ((color >> 8) & 0xFF);
                canvas->fillRect(top_left_x + x * scale, top_left_y + y * scale, scale, scale, unswapped);
            }
        }
    }
}

void VirtualDogApp::_draw_pixel_bone(int x, int y, int bob)
{
    auto* canvas = _data.hal->canvas;
    int by = y + bob;

    // Outer bone knobs (shadow / outline: dark brown 0x3E2723)
    canvas->fillCircle(x - 12, by - 3, 4, (uint32_t)0x3E2723);
    canvas->fillCircle(x - 12, by + 3, 4, (uint32_t)0x3E2723);
    canvas->fillCircle(x + 12, by - 3, 4, (uint32_t)0x3E2723);
    canvas->fillCircle(x + 12, by + 3, 4, (uint32_t)0x3E2723);

    // Bone shaft outline
    canvas->fillRect(x - 10, by - 4, 20, 9, (uint32_t)0x3E2723);

    // Inner bone knobs (Ivory 0xFFF8E7)
    canvas->fillCircle(x - 12, by - 3, 3, (uint32_t)0xFFF8E7);
    canvas->fillCircle(x - 12, by + 3, 3, (uint32_t)0xFFF8E7);
    canvas->fillCircle(x + 12, by - 3, 3, (uint32_t)0xFFF8E7);
    canvas->fillCircle(x + 12, by + 3, 3, (uint32_t)0xFFF8E7);

    // Inner bone shaft (Ivory 0xFFF8E7)
    canvas->fillRect(x - 10, by - 3, 20, 7, (uint32_t)0xFFF8E7);

    // Glossy white highlight on top edge
    canvas->drawFastHLine(x - 7, by - 2, 14, (uint32_t)0xFFFFFF);
}

void VirtualDogApp::_spawn_particle(uint8_t type, float x, float y)
{
    Particle_t p;
    p.x = x;
    p.y = y;
    p.type = type;
    p.life = 0;
    p.max_life = 25 + rand() % 15;

    if (type == 0) // Heart
    {
        p.vx = ((rand() % 100) - 50) / 70.0f;
        p.vy = -1.2f - (rand() % 100) / 80.0f;
    }
    else if (type == 1) // Bubble
    {
        p.vx = ((rand() % 100) - 50) / 60.0f;
        p.vy = -1.0f - (rand() % 100) / 90.0f;
    }
    else if (type == 2) // Zzz
    {
        p.vx = 0.6f + (rand() % 50) / 80.0f;
        p.vy = -0.9f;
    }
    else // Star
    {
        p.vx = ((rand() % 100) - 50) / 40.0f;
        p.vy = -1.5f - (rand() % 100) / 60.0f;
    }

    _data.particles.push_back(p);
}

void VirtualDogApp::_update_particles()
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

void VirtualDogApp::_draw_particles()
{
    auto* canvas = _data.hal->canvas;

    for (const auto& p : _data.particles)
    {
        int px = (int)p.x;
        int py = (int)p.y;

        if (p.type == 0) // Pixel Heart
        {
            canvas->fillCircle(px - 2, py - 2, 2, (uint32_t)0xFF4081);
            canvas->fillCircle(px + 2, py - 2, 2, (uint32_t)0xFF4081);
            canvas->fillTriangle(px - 4, py - 1, px + 4, py - 1, px, py + 4, (uint32_t)0xFF4081);
        }
        else if (p.type == 1) // Soap Bubble
        {
            canvas->drawCircle(px, py, 4, (uint32_t)0x80D8FF);
            canvas->drawPixel(px - 1, py - 1, (uint32_t)0xFFFFFF);
        }
        else if (p.type == 2) // Zzz
        {
            canvas->setTextDatum(textdatum_t::middle_center);
            canvas->setTextColor((uint32_t)0x80D8FF);
            canvas->setFont(&fonts::Font0);
            canvas->drawString("Z", px, py);
        }
        else if (p.type == 3) // Sparkle Star
        {
            canvas->fillCircle(px, py, 2, (uint32_t)0xFFD700);
            canvas->drawFastHLine(px - 3, py, 7, (uint32_t)0xFFFFFF);
            canvas->drawFastVLine(px, py - 3, 7, (uint32_t)0xFFFFFF);
        }
    }
}

void VirtualDogApp::_show_toast(const std::string& msg, uint32_t color)
{
    _data.toast_msg = msg;
    _data.toast_start_time = millis();
    _data.toast_color = color;
}

void VirtualDogApp::_draw_toast()
{
    if (_data.toast_msg.empty()) return;

    uint32_t elapsed = millis() - _data.toast_start_time;
    if (elapsed > 1800)
    {
        _data.toast_msg.clear();
        return;
    }

    auto* canvas = _data.hal->canvas;
    canvas->fillRoundRect(45, 42, 150, 22, 11, (uint32_t)0x101418);
    canvas->drawRoundRect(45, 42, 150, 22, 11, _data.toast_color);
    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(_data.toast_color);
    canvas->setFont(&fonts::Font0);
    canvas->drawString(_data.toast_msg.c_str(), 120, 53);
}

/* ---------------- Audio Sound Effects ---------------- */

void VirtualDogApp::_sound_bark()
{
    if (_data.hal == nullptr) return;
    _data.hal->buzz.tone(3000, 35);
    delay(40);
    _data.hal->buzz.tone(4200, 50);
}

void VirtualDogApp::_sound_happy_bark()
{
    if (_data.hal == nullptr) return;
    _data.hal->buzz.tone(3200, 30);
    delay(35);
    _data.hal->buzz.tone(4400, 45);
    delay(45);
    _data.hal->buzz.tone(4800, 60);
}

void VirtualDogApp::_sound_eat()
{
    if (_data.hal == nullptr) return;
    _data.hal->buzz.tone(1400, 20);
    delay(30);
    _data.hal->buzz.tone(1800, 20);
    delay(30);
    _data.hal->buzz.tone(1200, 25);
}

void VirtualDogApp::_sound_bounce()
{
    if (_data.hal == nullptr) return;
    _data.hal->buzz.tone(950, 15);
    delay(20);
    _data.hal->buzz.tone(1400, 18);
}

void VirtualDogApp::_sound_purr()
{
    if (_data.hal == nullptr) return;
    _data.hal->buzz.tone(1800, 35);
    delay(30);
    _data.hal->buzz.tone(2200, 40);
}

void VirtualDogApp::_sound_fanfare()
{
    if (_data.hal == nullptr) return;
    _data.hal->buzz.tone(2093, 40);
    delay(45);
    _data.hal->buzz.tone(2637, 40);
    delay(45);
    _data.hal->buzz.tone(3136, 45);
    delay(50);
    _data.hal->buzz.tone(4186, 90);
}

void VirtualDogApp::_sound_click()
{
    if (_data.hal == nullptr) return;
    _data.hal->buzz.tone(3400, 8);
}

void VirtualDogApp::_sound_pop()
{
    if (_data.hal == nullptr) return;
    _data.hal->buzz.tone(2600, 12);
    delay(15);
    _data.hal->buzz.tone(3200, 15);
}

void MOONCAKE::USER_APP::run_virtual_dog_app(HAL::HAL* hal)
{
    if (hal == nullptr) return;
    VirtualDogApp app;
    app.setUserData((void*)hal);
    app.onSetup();
    app.onCreate();
    while (1)
    {
        app.onRunning();
        if (app.isGoingDestroy())
        {
            app.resetGoingDestroyFlag();
            app.onDestroy();
            break;
        }
    }
}
