/**
 * @file app_pc_control.cpp
 * @brief Smartwatch-grade PC Control Center App implementation
 */

#include "app_pc_control.h"
#include "../common_define.h"
#include "../app_wifi_scan/pc_control_config.h"
#include "../utilities/wifi_manager/wifi_manager.h"
#include <esp_http_client.h>
#include <cJSON.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

using namespace MOONCAKE::USER_APP;

struct HttpBuffer {
    char data[512];
    int len;
};

static esp_err_t _http_event_cb(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        HttpBuffer* buf = (HttpBuffer*)evt->user_data;
        if (buf && evt->data && evt->data_len > 0) {
            int copy_len = std::min(evt->data_len, (int)(sizeof(buf->data) - 1 - buf->len));
            if (copy_len > 0) {
                memcpy(buf->data + buf->len, evt->data, copy_len);
                buf->len += copy_len;
                buf->data[buf->len] = '\0';
            }
        }
    }
    return ESP_OK;
}

void PC_Control::_parse_status_json(const std::string& json_str)
{
    cJSON* root = cJSON_Parse(json_str.c_str());
    if (root == nullptr) return;

    cJSON* j_vol = cJSON_GetObjectItem(root, "volume");
    if (j_vol != nullptr && cJSON_IsNumber(j_vol)) {
        _data.local_volume = std::max(0, std::min(100, j_vol->valueint));
    }

    cJSON* j_br = cJSON_GetObjectItem(root, "brightness");
    if (j_br != nullptr && cJSON_IsNumber(j_br)) {
        _data.local_brightness = std::max(0, std::min(100, j_br->valueint));
    }

    cJSON* j_muted = cJSON_GetObjectItem(root, "muted");
    if (j_muted != nullptr && cJSON_IsBool(j_muted)) {
        _data.local_muted = cJSON_IsTrue(j_muted);
    }

    cJSON_Delete(root);
}

static const char* _get_server_base_url() {
    if (WIFI_MANAGER::connectedSSID() == PC_WIFI_SSID_ALT) {
        return PC_CONTROL_URL_ALT;
    }
    return PC_CONTROL_BASE_URL;
}

static void _trigger_wifi_connect() {
    static int attempt_idx = 0;
    if (attempt_idx % 2 == 0) {
        WIFI_MANAGER::connect(PC_WIFI_SSID, PC_WIFI_PASSWORD);
    } else {
        WIFI_MANAGER::connect(PC_WIFI_SSID_ALT, PC_WIFI_PASS_ALT);
    }
    attempt_idx++;
}

int PC_Control::_fetch_status()
{
    if (WIFI_MANAGER::state() != WIFI_MANAGER::State::CONNECTED) {
        return -1;
    }

    char url_buf[128];
    snprintf(url_buf, sizeof(url_buf), "%s/api/status", _get_server_base_url());

    HttpBuffer resp_buf = {};
    esp_http_client_config_t cfg = {};
    cfg.url = url_buf;
    cfg.method = HTTP_METHOD_GET;
    cfg.timeout_ms = 1500;
    cfg.event_handler = _http_event_cb;
    cfg.user_data = &resp_buf;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == nullptr) return -2;

    esp_http_client_set_header(client, "X-Controller-Token", PC_CONTROLLER_TOKEN);

    esp_err_t result = esp_http_client_perform(client);
    int code = (result == ESP_OK) ? esp_http_client_get_status_code(client) : 0;
    esp_http_client_cleanup(client);

    if (result == ESP_OK && code == 200 && resp_buf.len > 0) {
        _parse_status_json(resp_buf.data);
        _data.has_synced_status = true;
        _render();
    }
    return code;
}

int PC_Control::_send_command(const char* cmd, int val)
{
    if (WIFI_MANAGER::state() != WIFI_MANAGER::State::CONNECTED) {
        _trigger_wifi_connect();
        _data.toast_msg = "Reconnecting Wi-Fi...";
        _data.toast_until = millis() + 1800;
        return -1;
    }

    char body[128];
    snprintf(body, sizeof(body), "{\"command\":\"%s\",\"value\":%d}", cmd, val);

    char url_buf[128];
    snprintf(url_buf, sizeof(url_buf), "%s/api/command", _get_server_base_url());

    HttpBuffer resp_buf = {};
    esp_http_client_config_t cfg = {};
    cfg.url = url_buf;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 1500;
    cfg.event_handler = _http_event_cb;
    cfg.user_data = &resp_buf;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == nullptr) {
        _data.toast_msg = "Client Init Error";
        _data.toast_until = millis() + 1800;
        return -2;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "X-Controller-Token", PC_CONTROLLER_TOKEN);
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t result = esp_http_client_perform(client);
    int code = (result == ESP_OK) ? esp_http_client_get_status_code(client) : 0;
    esp_http_client_cleanup(client);

    if (result == ESP_OK && code >= 200 && code < 300) {
        _data.toast_msg = "OK";
        if (resp_buf.len > 0) {
            _parse_status_json(resp_buf.data);
            _data.has_synced_status = true;
        }
    } else if (code == 401) {
        _data.toast_msg = "Auth Error (401)";
    } else if (code == 400) {
        _data.toast_msg = "Bad Request (400)";
    } else {
        _data.toast_msg = "Server Offline";
    }
    _data.toast_until = millis() + 1500;
    return code;
}

void PC_Control::_render()
{
    auto canvas = _data.hal->canvas;
    canvas->fillScreen(TFT_BLACK);
    canvas->setTextDatum(textdatum_t::middle_center);

    // 1. Top Status Indicator (clean smartwatch style without on-screen back button)
    auto wstate = WIFI_MANAGER::state();
    if (wstate == WIFI_MANAGER::State::CONNECTED) {
        canvas->fillCircle(98, 24, 4, TFT_GREEN);
        canvas->setFont(GUI_FONT_CN_SMALL);
        canvas->setTextColor((uint32_t)0x86EFAC, TFT_BLACK);
        canvas->drawString("PC Online", 126, 24);
    } else if (wstate == WIFI_MANAGER::State::CONNECTING) {
        canvas->fillCircle(98, 24, 4, TFT_YELLOW);
        canvas->setFont(GUI_FONT_CN_SMALL);
        canvas->setTextColor((uint32_t)0xFDE047, TFT_BLACK);
        canvas->drawString("Conn...", 126, 24);
    } else {
        canvas->fillCircle(98, 24, 4, TFT_RED);
        canvas->setFont(GUI_FONT_CN_SMALL);
        canvas->setTextColor((uint32_t)0xFCA5A5, TFT_BLACK);
        canvas->drawString("Offline", 126, 24);
    }

    // 3. Card Title & Subtitle
    struct CardMeta {
        const char* title;
        const char* subtitle;
        uint32_t accent;
    };
    static const CardMeta metas[5] = {
        {"VOLUME", "PC Master Sound", (uint32_t)0x008CD6},
        {"BRIGHTNESS", "Display Backlight", (uint32_t)0xEA580C},
        {"MEDIA", "Now Playing", (uint32_t)0x16A34A},
        {"BROWSER ZOOM", "Web Page Scale", (uint32_t)0x7C3AED},
        {"APP SWITCH", "Window & Tabs", (uint32_t)0xDB2777}
    };

    const auto& meta = metas[_data.current_card];
    canvas->setFont(GUI_FONT_CN_BIG);
    canvas->setTextColor(meta.accent, TFT_BLACK);
    canvas->drawString(meta.title, 120, 52);

    canvas->setFont(GUI_FONT_CN_SMALL);
    canvas->setTextColor((uint32_t)0x94A3B8, TFT_BLACK);
    canvas->drawString(meta.subtitle, 120, 74);

    // Helper: Draw crisp vector "-" button
    auto draw_minus_button = [&](int x, int y, int r) {
        canvas->fillSmoothCircle(x, y, r, (uint32_t)0x1E293B);
        canvas->drawCircle(x, y, r, (uint32_t)0x475569);
        canvas->fillRect(x - 8, y - 2, 16, 4, TFT_WHITE);
    };

    // Helper: Draw crisp vector "+" button
    auto draw_plus_button = [&](int x, int y, int r) {
        canvas->fillSmoothCircle(x, y, r, (uint32_t)0x1E293B);
        canvas->drawCircle(x, y, r, (uint32_t)0x475569);
        canvas->fillRect(x - 8, y - 2, 16, 4, TFT_WHITE);
        canvas->fillRect(x - 2, y - 8, 4, 16, TFT_WHITE);
    };

    // 4. Card Content
    if (_data.current_card == 0) { // Volume
        draw_minus_button(48, 120, 24);

        // Center Volume circle
        uint32_t ring_color = _data.local_muted ? (uint32_t)0xDC2626 : meta.accent;
        canvas->fillSmoothCircle(120, 120, 36, (uint32_t)0x0F172A);
        canvas->drawCircle(120, 120, 36, ring_color);
        canvas->drawCircle(120, 120, 35, ring_color);

        if (_data.local_muted) {
            canvas->setTextColor((uint32_t)0xF87171, (uint32_t)0x0F172A);
            canvas->setFont(GUI_FONT_CN_BIG);
            canvas->drawString("MUTED", 120, 120);
        } else {
            char vol_str[16];
            snprintf(vol_str, sizeof(vol_str), "%d%%", _data.local_volume);
            canvas->setTextColor(TFT_WHITE, (uint32_t)0x0F172A);
            canvas->setFont(GUI_FONT_CN_BIG);
            canvas->drawString(vol_str, 120, 120);
        }

        draw_plus_button(192, 120, 24);

    } else if (_data.current_card == 1) { // Brightness
        draw_minus_button(48, 120, 24);

        canvas->fillSmoothCircle(120, 120, 36, (uint32_t)0x0F172A);
        canvas->drawCircle(120, 120, 36, meta.accent);
        canvas->drawCircle(120, 120, 35, meta.accent);

        char br_str[16];
        snprintf(br_str, sizeof(br_str), "%d%%", _data.local_brightness);
        canvas->setTextColor(TFT_WHITE, (uint32_t)0x0F172A);
        canvas->setFont(GUI_FONT_CN_BIG);
        canvas->drawString(br_str, 120, 120);

        draw_plus_button(192, 120, 24);

    } else if (_data.current_card == 2) { // Media
        // Prev button
        canvas->fillSmoothCircle(48, 120, 24, (uint32_t)0x1E293B);
        canvas->drawCircle(48, 120, 24, (uint32_t)0x475569);
        canvas->fillRect(40, 114, 3, 12, TFT_WHITE);
        canvas->fillTriangle(53, 114, 53, 126, 44, 120, TFT_WHITE);

        // Center Play/Pause button
        canvas->fillSmoothCircle(120, 120, 36, meta.accent);
        canvas->fillTriangle(114, 108, 114, 132, 131, 120, TFT_WHITE);

        // Next button
        canvas->fillSmoothCircle(192, 120, 24, (uint32_t)0x1E293B);
        canvas->drawCircle(192, 120, 24, (uint32_t)0x475569);
        canvas->fillRect(197, 114, 3, 12, TFT_WHITE);
        canvas->fillTriangle(187, 114, 187, 126, 196, 120, TFT_WHITE);

    } else if (_data.current_card == 3) { // Browser Zoom
        // Zoom -
        canvas->fillRoundRect(18, 104, 62, 34, 10, (uint32_t)0x1E293B);
        canvas->drawRoundRect(18, 104, 62, 34, 10, (uint32_t)0x475569);
        canvas->setFont(GUI_FONT_CN_SMALL);
        canvas->setTextColor(TFT_WHITE, (uint32_t)0x1E293B);
        canvas->drawString("Zoom -", 49, 121);

        // Center 100% Reset
        canvas->fillSmoothCircle(120, 120, 32, (uint32_t)0x0F172A);
        canvas->drawCircle(120, 120, 32, meta.accent);
        canvas->setFont(GUI_FONT_CN_BIG);
        canvas->setTextColor(TFT_WHITE, (uint32_t)0x0F172A);
        canvas->drawString("100%", 120, 120);

        // Zoom +
        canvas->fillRoundRect(160, 104, 62, 34, 10, (uint32_t)0x1E293B);
        canvas->drawRoundRect(160, 104, 62, 34, 10, (uint32_t)0x475569);
        canvas->setFont(GUI_FONT_CN_SMALL);
        canvas->setTextColor(TFT_WHITE, (uint32_t)0x1E293B);
        canvas->drawString("Zoom +", 191, 121);

    } else if (_data.current_card == 4) { // App Switcher & Tabs
        // Tab <
        canvas->fillRoundRect(16, 104, 64, 34, 10, (uint32_t)0x1E293B);
        canvas->drawRoundRect(16, 104, 64, 34, 10, (uint32_t)0x475569);
        canvas->setFont(GUI_FONT_CN_SMALL);
        canvas->setTextColor(TFT_WHITE, (uint32_t)0x1E293B);
        canvas->drawString("Tab <", 48, 121);

        // Switch App (Alt+Tab)
        canvas->fillSmoothCircle(120, 120, 34, meta.accent);
        canvas->setFont(GUI_FONT_CN_BIG);
        canvas->setTextColor(TFT_WHITE, meta.accent);
        canvas->drawString("SWITCH", 120, 120);

        // Tab >
        canvas->fillRoundRect(160, 104, 64, 34, 10, (uint32_t)0x1E293B);
        canvas->drawRoundRect(160, 104, 64, 34, 10, (uint32_t)0x475569);
        canvas->setFont(GUI_FONT_CN_SMALL);
        canvas->setTextColor(TFT_WHITE, (uint32_t)0x1E293B);
        canvas->drawString("Tab >", 192, 121);
    }

    // 5. Smartwatch Card Indicator Dots (at y = 175)
    for (int i = 0; i < 5; i++) {
        int dot_x = 96 + i * 12;
        if (i == _data.current_card) {
            canvas->fillSmoothCircle(dot_x, 175, 4, meta.accent);
        } else {
            canvas->fillSmoothCircle(dot_x, 175, 2, (uint32_t)0x475569);
        }
    }

    // 6. Status Toast / Message
    canvas->setFont(GUI_FONT_CN_SMALL);
    if (millis() < _data.toast_until && !_data.toast_msg.empty()) {
        canvas->setTextColor((uint32_t)0xFCD34D, TFT_BLACK);
        canvas->drawString(_data.toast_msg.c_str(), 120, 204);
    } else {
        canvas->setTextColor((uint32_t)0x64748B, TFT_BLACK);
        canvas->drawString("Swipe: Back | ^ v: Card", 120, 204);
    }

    canvas->pushSprite(0, 0);
}

void PC_Control::_handle_encoder()
{
    // Rotary Dial Rotation: Clockwise = increment, Counter-Clockwise = decrement
    if (_data.hal->encoder.wasMoved(true)) {
        bool is_clockwise = (_data.hal->encoder.getDirection() < 1);
        _data.hal->buzz.tone(3600, 15);

        if (_data.current_card == 0) { // Volume
            _data.local_volume = is_clockwise ? std::min(100, _data.local_volume + 5) : std::max(0, _data.local_volume - 5);
            _data.local_muted = false;
            _render();
            _send_command("volume_delta", is_clockwise ? 5 : -5);
        } else if (_data.current_card == 1) { // Brightness
            _data.local_brightness = is_clockwise ? std::min(100, _data.local_brightness + 5) : std::max(0, _data.local_brightness - 5);
            _render();
            _send_command("brightness_delta", is_clockwise ? 5 : -5);
        } else if (_data.current_card == 2) { // Media
            _render();
            _send_command(is_clockwise ? "media_next" : "media_previous", 0);
        } else if (_data.current_card == 3) { // Zoom
            _render();
            _send_command(is_clockwise ? "zoom_in" : "zoom_out", 0);
        } else if (_data.current_card == 4) { // Navigation Scroll
            _render();
            _send_command(is_clockwise ? "scroll_down" : "scroll_up", 0);
        }
        _render();
    }

    // Encoder Button Press
    if (!_data.hal->encoder.btn.read()) {
        uint32_t press_start = millis();
        bool is_long_press = false;

        while (!_data.hal->encoder.btn.read()) {
            delay(10);
            if (millis() - press_start > 700) {
                // Long press = Exit back to launcher
                _data.hal->buzz.tone(2800, 40);
                is_long_press = true;
                while (!_data.hal->encoder.btn.read()) delay(10);
                destroyApp();
                return;
            }
        }

        if (!is_long_press) {
            _data.hal->buzz.tone(4000, 25);
            if (_data.current_card == 0) {
                _data.local_muted = !_data.local_muted;
                _render();
                _send_command("mute", 0);
            } else if (_data.current_card == 1) {
                _data.local_brightness = 50;
                _render();
                _send_command("brightness_delta", 0);
            } else if (_data.current_card == 2) {
                _send_command("media_play_pause", 0);
            } else if (_data.current_card == 3) {
                _send_command("zoom_reset", 0);
            } else if (_data.current_card == 4) {
                _send_command("app_switch", 0);
            }
            _render();
        }
    }
}

void PC_Control::_handle_touch()
{
    if (!_data.hal->tp.isTouched()) return;

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

    // 1. Swipe Left -> Go Back to Previous Screen (Launcher)
    if (dx < -40 && abs(dy) < 55) {
        _data.hal->buzz.tone(3000, 30);
        destroyApp();
        return;
    }

    // 2. Swipe Right -> Navigate to Next Card / Mode
    if (dx > 40 && abs(dy) < 55) {
        _data.hal->buzz.tone(3600, 20);
        _data.current_card = (_data.current_card + 1) % 5;
        _render();
        return;
    }

    // 2. Swipe Up -> Next Card
    if (dy < -35 && abs(dx) < 50) {
        _data.hal->buzz.tone(3600, 20);
        _data.current_card = (_data.current_card + 1) % 5;
        _render();
        return;
    }

    // 3. Swipe Down -> Previous Card
    if (dy > 35 && abs(dx) < 50) {
        _data.hal->buzz.tone(3600, 20);
        _data.current_card = (_data.current_card - 1 + 5) % 5;
        _render();
        return;
    }

    // 4. Smartwatch Tap Detection on Cards
    if (abs(dx) <= 22 && abs(dy) <= 22 && duration < 550) {
        _data.hal->buzz.tone(4000, 12);

        if (_data.current_card == 0) { // Volume Card Taps
            int d_left = (start_x - 48) * (start_x - 48) + (start_y - 120) * (start_y - 120);
            int d_right = (start_x - 192) * (start_x - 192) + (start_y - 120) * (start_y - 120);
            int d_center = (start_x - 120) * (start_x - 120) + (start_y - 120) * (start_y - 120);

            if (d_left <= 28 * 28) {
                _data.local_volume = std::max(0, _data.local_volume - 5);
                _data.local_muted = false;
                _render();
                _send_command("volume_delta", -5);
            } else if (d_right <= 28 * 28) {
                _data.local_volume = std::min(100, _data.local_volume + 5);
                _data.local_muted = false;
                _render();
                _send_command("volume_delta", 5);
            } else if (d_center <= 36 * 36) {
                _data.local_muted = !_data.local_muted;
                _render();
                _send_command("mute", 0);
            }
        } else if (_data.current_card == 1) { // Brightness Card Taps
            int d_left = (start_x - 48) * (start_x - 48) + (start_y - 120) * (start_y - 120);
            int d_right = (start_x - 192) * (start_x - 192) + (start_y - 120) * (start_y - 120);

            if (d_left <= 28 * 28) {
                _data.local_brightness = std::max(0, _data.local_brightness - 5);
                _render();
                _send_command("brightness_delta", -5);
            } else if (d_right <= 28 * 28) {
                _data.local_brightness = std::min(100, _data.local_brightness + 5);
                _render();
                _send_command("brightness_delta", 5);
            }
        } else if (_data.current_card == 2) { // Media Card Taps
            int d_center = (start_x - 120) * (start_x - 120) + (start_y - 120) * (start_y - 120);
            int d_prev = (start_x - 48) * (start_x - 48) + (start_y - 120) * (start_y - 120);
            int d_next = (start_x - 192) * (start_x - 192) + (start_y - 120) * (start_y - 120);

            if (d_center <= 36 * 36) {
                _send_command("media_play_pause", 0);
            } else if (d_prev <= 26 * 26) {
                _send_command("media_previous", 0);
            } else if (d_next <= 26 * 26) {
                _send_command("media_next", 0);
            }
        } else if (_data.current_card == 3) { // Zoom Card Taps
            int d_center = (start_x - 120) * (start_x - 120) + (start_y - 120) * (start_y - 120);
            if (start_x >= 18 && start_x <= 80 && start_y >= 95 && start_y <= 145) {
                _send_command("zoom_out", 0);
            } else if (start_x >= 158 && start_x <= 222 && start_y >= 95 && start_y <= 145) {
                _send_command("zoom_in", 0);
            } else if (d_center <= 34 * 34) {
                _send_command("zoom_reset", 0);
            }
        } else if (_data.current_card == 4) { // Navigation Card Taps (Tab / Switch)
            int d_center = (start_x - 120) * (start_x - 120) + (start_y - 120) * (start_y - 120);
            if (start_x >= 14 && start_x <= 80 && start_y >= 95 && start_y <= 145) {
                _send_command("browser_tab_previous", 0);
            } else if (start_x >= 160 && start_x <= 226 && start_y >= 95 && start_y <= 145) {
                _send_command("browser_tab_next", 0);
            } else if (d_center <= 36 * 36) {
                _send_command("app_switch", 0);
            }
        }
        _render();
    }
}

void PC_Control::onSetup()
{
    setAppName("PC Control");
    setAllowBgRunning(false);

    PC_Control::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void PC_Control::onCreate()
{
    // Auto-check Wi-Fi state on entry
    if (WIFI_MANAGER::state() == WIFI_MANAGER::State::NOT_INITIALIZED) {
        WIFI_MANAGER::init();
    }
    if (WIFI_MANAGER::state() == WIFI_MANAGER::State::DISCONNECTED) {
        _trigger_wifi_connect();
    } else if (WIFI_MANAGER::state() == WIFI_MANAGER::State::CONNECTED) {
        _fetch_status();
    }
    _render();
}

void PC_Control::onRunning()
{
    _handle_encoder();
    if (isGoingDestroy()) return;

    _handle_touch();
    if (isGoingDestroy()) return;

    // Periodic Wi-Fi state refresh & status sync
    if ((millis() - _data.last_wifi_refresh) > 2000) {
        _data.last_wifi_refresh = millis();
        if (WIFI_MANAGER::state() == WIFI_MANAGER::State::CONNECTED) {
            if (!_data.has_synced_status || (millis() - _data.last_status_fetch) > 5000) {
                _data.last_status_fetch = millis();
                _fetch_status();
            }
        } else if (WIFI_MANAGER::state() == WIFI_MANAGER::State::DISCONNECTED ||
                   WIFI_MANAGER::state() == WIFI_MANAGER::State::ERROR) {
            if ((millis() - _data.last_status_fetch) > 4000) {
                _data.last_status_fetch = millis();
                _trigger_wifi_connect();
            }
        }
        _render();
    }

    delay(15);
}

void PC_Control::onDestroy()
{
    if (_data.hal != nullptr) {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_pc_control_center(HAL::HAL* hal)
{
    PC_Control app;
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
