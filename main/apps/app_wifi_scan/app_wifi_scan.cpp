
#include "app_wifi_scan.h"
#include "../common_define.h"

#include <algorithm>
#include <cstdio>

using namespace MOONCAKE::USER_APP;

namespace {

const char* passwordCharset =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789!@#$%&*()-_=+[]{};:,.?/";

constexpr int passwordCharsetLength =
    sizeof(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789!@#$%&*()-_=+[]{};:,.?/"
    ) - 1;

constexpr int passwordActionCount = 4;

} // namespace

void WiFi_Scan::_scan_wifi() {
    _data.status_message = "Scanning...";

    _gui.renderNetworks(
        _data.networks,
        _data.selected_network,
        _data.status_message
    );

    if (!WIFI_MANAGER::scan(_data.networks)) {
        _data.networks.clear();
        _data.status_message = "Scan failed - press to retry";
    } else if (_data.networks.empty()) {
        _data.status_message = "No networks found";
    } else {
        _data.status_message = "Select network; * = secured";
    }

    _data.selected_network = 0;
    _data.menu_y_offset = 0;
    _data.page = WIFI_SCAN::Page::NETWORKS;

    _log(
        "Wi-Fi scan returned %u networks",
        static_cast<unsigned>(_data.networks.size())
    );
}

void WiFi_Scan::_render() {
    switch (_data.page) {
        case WIFI_SCAN::Page::NETWORKS:
            _gui.renderNetworks(
                _data.networks,
                _data.selected_network,
                _data.status_message
            );
            break;

        case WIFI_SCAN::Page::PASSWORD:
            if (_data.selected_network >= 0 &&
                _data.selected_network <
                    static_cast<int>(_data.networks.size())) {
                _gui.renderPassword(
                    _data.networks[_data.selected_network].ssid,
                    _data.password,
                    _data.selected_char,
                    _data.status_message
                );
            }
            break;

        case WIFI_SCAN::Page::STATUS: {
            const auto state = WIFI_MANAGER::state();

            const char* status = "Disconnected";

            if (state == WIFI_MANAGER::State::CONNECTING) {
                status = "Connecting...";
            } else if (state == WIFI_MANAGER::State::CONNECTED) {
                status = "Connected";
            } else if (state == WIFI_MANAGER::State::ERROR) {
                status = "Connection error";
            }

            _gui.renderStatus(
                WIFI_MANAGER::connectedSSID(),
                status,
                WIFI_MANAGER::ipv4(),
                _data.status_message
            );
            break;
        }
    }
}

void WiFi_Scan::_connect_selected() {
    if (_data.selected_network < 0 ||
        _data.selected_network >=
            static_cast<int>(_data.networks.size())) {
        _data.status_message = "Select a network first";
        return;
    }

    const auto& network = _data.networks[_data.selected_network];

    _data.password.clear();

    if (network.secure) {
        _data.selected_char = 0;
        _data.status_message =
            "Rotate for chars/actions; press to select";
        _data.page = WIFI_SCAN::Page::PASSWORD;
        return;
    }

    if (WIFI_MANAGER::connect(network.ssid, "")) {
        _data.status_message = "Connecting; waiting for IP";
    } else {
        _data.status_message = WIFI_MANAGER::lastError();
    }

    _data.page = WIFI_SCAN::Page::STATUS;
}

void WiFi_Scan::_handle_encoder() {
    if (_data.hal == nullptr) {
        return;
    }

    if (_data.hal->encoder.wasMoved(true)) {
        const int direction =
            _data.hal->encoder.getDirection() < 1 ? 1 : -1;

        if (_data.page == WIFI_SCAN::Page::NETWORKS) {
            const int count =
                static_cast<int>(_data.networks.size());

            if (count > 0) {
                _data.selected_network =
                    (_data.selected_network + direction + count) % count;
            }
        } else if (_data.page == WIFI_SCAN::Page::PASSWORD) {
            const int total =
                passwordCharsetLength + passwordActionCount;

            _data.selected_char =
                (_data.selected_char + direction + total) % total;
        } else {
            _data.page = WIFI_SCAN::Page::NETWORKS;
            _data.status_message = "Select network; * = secured";
        }
    }

    // Assumes read() returns false when the button is pressed.
    const bool button_down = !_data.hal->encoder.btn.read();

    const bool button_clicked =
        button_down && !_data.button_was_down;

    _data.button_was_down = button_down;

    if (!button_clicked) {
        return;
    }

    if (_data.page == WIFI_SCAN::Page::NETWORKS) {
        if (_data.networks.empty()) {
            _scan_wifi();
        } else {
            _connect_selected();
        }
        return;
    }

    if (_data.page == WIFI_SCAN::Page::PASSWORD) {
        if (_data.selected_network < 0 ||
            _data.selected_network >=
                static_cast<int>(_data.networks.size())) {
            _data.page = WIFI_SCAN::Page::NETWORKS;
            _data.status_message = "Select a network";
            return;
        }

        if (_data.selected_char < passwordCharsetLength) {
            if (_data.password.size() < 63) {
                _data.password.push_back(
                    passwordCharset[_data.selected_char]
                );
                _data.status_message = "Character added";
            } else {
                _data.status_message = "Password limit reached";
            }
            return;
        }

        const int action =
            _data.selected_char - passwordCharsetLength;

        switch (action) {
            case 0:
                if (!_data.password.empty()) {
                    _data.password.pop_back();
                }
                _data.status_message = "Backspace";
                break;

            case 1:
                _data.password.clear();
                _data.status_message = "Password cleared";
                break;

            case 2: {
                if (_data.password.size() < 8 ||
                    _data.password.size() > 63) {
                    _data.status_message = "Use 8-63 characters";
                    break;
                }

                const std::string ssid =
                    _data.networks[_data.selected_network].ssid;

                if (WIFI_MANAGER::connect(ssid, _data.password)) {
                    _data.status_message = "Connecting; waiting for IP";
                } else {
                    _data.status_message = WIFI_MANAGER::lastError();
                }

                _data.page = WIFI_SCAN::Page::STATUS;
                break;
            }

            case 3:
                _data.password.clear();
                _data.selected_char = 0;
                _data.page = WIFI_SCAN::Page::NETWORKS;
                _data.status_message = "Choose a network";
                break;

            default:
                break;
        }

        return;
    }

    destroyApp();
}

void WiFi_Scan::onSetup() {
    setAllowBgRunning(false);

    WIFI_SCAN::Data_t default_data;
    _data = default_data;

    _data.hal = static_cast<HAL::HAL*>(getUserData());
}

void WiFi_Scan::onCreate() {
    _log("Wi-Fi manager onCreate");

    _data.button_was_down = false;

    if (!WIFI_MANAGER::init()) {
        _data.status_message = WIFI_MANAGER::lastError();
        _data.page = WIFI_SCAN::Page::STATUS;
    } else {
        _scan_wifi();
    }

    _data.menu_y_offset_anim.setAnim(
        LVGL::overshoot,
        100,
        0,
        600
    );

    _data.menu_y_offset_anim.resetTime(millis());
}

void WiFi_Scan::onRunning() {
    _handle_encoder();

    /* Smartwatch Touch Handling for Wi-Fi Scan */
    if (_data.hal != nullptr && _data.hal->tp.isTouched()) {
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

        // Universal Smartwatch Swipe Left OR Swipe Right -> Back to previous page or Launcher
        if ((dx < -40 && abs(dy) < 55) || (dx > 40 && abs(dy) < 55)) {
            _data.hal->buzz.tone(3000, 30);
            if (_data.page == WIFI_SCAN::Page::PASSWORD) {
                _data.page = WIFI_SCAN::Page::NETWORKS;
                _data.status_message = "Select network; * = secured";
                _render();
                return;
            } else {
                destroyApp();
                return;
            }
        }

        // 3. Swipe Up / Down to scroll Wi-Fi networks
        if (_data.page == WIFI_SCAN::Page::NETWORKS && !_data.networks.empty()) {
            if (dy < -30 && abs(dx) < 50) {
                _data.hal->buzz.tone(3600, 10);
                const int count = static_cast<int>(_data.networks.size());
                _data.selected_network = (_data.selected_network + 1) % count;
                _render();
                return;
            } else if (dy > 30 && abs(dx) < 50) {
                _data.hal->buzz.tone(3600, 10);
                const int count = static_cast<int>(_data.networks.size());
                _data.selected_network = (_data.selected_network - 1 + count) % count;
                _render();
                return;
            }
        }

        // 4. Tap directly on a Wi-Fi network row to select & connect
        if (_data.page == WIFI_SCAN::Page::NETWORKS && abs(dx) <= 25 && abs(dy) <= 25 && duration < 550) {
            if (start_y >= 60 && start_y <= 195 && !_data.networks.empty()) {
                const int visible = 5;
                int start = 0;
                if (_data.selected_network >= visible) start = _data.selected_network - visible + 1;
                if (start + visible > static_cast<int>(_data.networks.size())) {
                    start = _data.networks.size() > visible ? _data.networks.size() - visible : 0;
                }
                int clicked_row = (start_y - 60) / 27;
                int target_idx = start + clicked_row;
                if (target_idx >= 0 && target_idx < static_cast<int>(_data.networks.size())) {
                    _data.hal->buzz.tone(4000, 15);
                    _data.selected_network = target_idx;
                    _connect_selected();
                    _render();
                    return;
                }
            } else if (_data.networks.empty()) {
                _data.hal->buzz.tone(4000, 15);
                _scan_wifi();
                return;
            }
        }
    }

    if (_data.page == WIFI_SCAN::Page::STATUS) {
        const auto state = WIFI_MANAGER::state();

        if (state == WIFI_MANAGER::State::CONNECTED) {
            _data.status_message =
                "Connected; IP: " + WIFI_MANAGER::ipv4();
        } else if (state == WIFI_MANAGER::State::CONNECTING) {
            _data.status_message = "Connecting; waiting for IP";
        } else if (state == WIFI_MANAGER::State::ERROR) {
            _data.status_message = WIFI_MANAGER::lastError();
        } else {
            _data.status_message = "Disconnected";
        }
    }

    _render();
    delay(20);
}

void WiFi_Scan::onDestroy() {
    _log("Wi-Fi manager onDestroy");
}
