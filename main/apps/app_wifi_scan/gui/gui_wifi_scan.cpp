#include "gui_wifi_scan.h"
#include <cstdio>

#define delay(ms) SDL_Delay(ms)

void GUI_WiFi_Scan::init() {
    _canvas->fillScreen(TFT_DARKGRAY);
    _canvas->fillSmoothCircle(120, 120, 120, _theme_color);
    _draw_top_icon();
    _draw_quit_button();
    _canvas->drawCircle(120, 120, 120, TFT_DARKGRAY);
    _canvas->pushSprite(0, 0);
}

void GUI_WiFi_Scan::renderNetworks(const std::vector<WIFI_MANAGER::Network>& networks, int selected, const std::string& message) {
    _canvas->fillScreen(_theme_color);
    _canvas->fillSmoothRoundRect(7, 8, 226, 224, 22, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->setFont(GUI_FONT_CN_BIG);
    _canvas->drawCenterString("WI-FI MANAGER", 120, 16);
    _canvas->setFont(GUI_FONT_CN_SMALL);
    _canvas->drawCenterString("Rotate: select | Press: choose", 120, 40);

    const int visible = 5;
    int start = 0;
    if (selected >= visible) start = selected - visible + 1;
    if (start + visible > static_cast<int>(networks.size())) start = networks.size() > visible ? networks.size() - visible : 0;
    for (int row = 0; row < visible; ++row) {
        int idx = start + row;
        if (idx >= static_cast<int>(networks.size())) break;
        int y = 61 + row * 27;
        if (idx == selected) _canvas->fillRoundRect(14, y - 2, 212, 24, 5, _theme_color);
        _canvas->setTextColor(idx == selected ? TFT_WHITE : _theme_color);
        char line[48];
        snprintf(line, sizeof(line), "%c %.17s %ddBm %s",
                 idx == selected ? '>' : ' ',
                 networks[idx].ssid.c_str(), networks[idx].rssi,
                 networks[idx].secure ? "*" : "O");
        _canvas->drawString(line, 20, y + 2);
    }
    _canvas->setTextColor(_theme_color);
    if (networks.empty()) _canvas->drawCenterString("No networks found", 120, 100);
    _canvas->drawCenterString(message.c_str(), 120, 205);
    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}

void GUI_WiFi_Scan::renderPassword(const std::string& ssid, const std::string& password, int selectedChar, const std::string& message) {
    static const char* actions[] = {"BACKSPACE", "CLEAR", "CONNECT", "CANCEL"};
    static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%&*()-_=+[]{};:,.?/";
    constexpr int charsetLen = sizeof(charset) - 1;
    _canvas->fillScreen(_theme_color);
    _canvas->fillSmoothRoundRect(7, 8, 226, 224, 22, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->setFont(GUI_FONT_CN_BIG);
    _canvas->drawCenterString("WI-FI PASSWORD", 120, 15);
    _canvas->setFont(GUI_FONT_CN_SMALL);
    char ssidLine[40];
    snprintf(ssidLine, sizeof(ssidLine), "Network: %.23s", ssid.c_str());
    _canvas->drawCenterString(ssidLine, 120, 39);

    std::string shown(password.size(), '*');
    shown.append(1, '_');
    _canvas->drawCenterString(shown.c_str(), 120, 62);
    int charSlot = selectedChar;
    if (charSlot < charsetLen) {
        char current[2] = {charset[charSlot], 0};
        _canvas->fillRoundRect(88, 87, 64, 31, 6, _theme_color);
        _canvas->setTextColor(TFT_WHITE);
        _canvas->drawCenterString(current, 120, 91);
    } else {
        int action = charSlot - charsetLen;
        if (action < 0 || action > 3) action = 0;
        _canvas->fillRoundRect(55, 87, 130, 31, 6, _theme_color);
        _canvas->setTextColor(TFT_WHITE);
        _canvas->drawCenterString(actions[action], 120, 91);
    }
    _canvas->setTextColor(_theme_color);
    _canvas->drawCenterString("Rotate: char/action", 120, 128);
    _canvas->drawCenterString("Press: add / run action", 120, 146);
    _canvas->drawCenterString(message.c_str(), 120, 183);
    _canvas->drawCenterString("Password hidden on screen", 120, 204);
    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}

void GUI_WiFi_Scan::renderStatus(const std::string& ssid, const std::string& status, const std::string& ip, const std::string& message) {
    _canvas->fillScreen(_theme_color);
    _canvas->fillSmoothRoundRect(7, 8, 226, 224, 22, TFT_WHITE);
    _canvas->setTextColor(_theme_color);
    _canvas->setFont(GUI_FONT_CN_BIG);
    _canvas->drawCenterString("WI-FI STATUS", 120, 18);
    _canvas->setFont(GUI_FONT_CN_SMALL);
    char line[48];
    snprintf(line, sizeof(line), "Network: %.23s", ssid.c_str());
    _canvas->drawCenterString(line, 120, 58);
    _canvas->drawCenterString(status.c_str(), 120, 88);
    if (!ip.empty()) {
        snprintf(line, sizeof(line), "IP: %s", ip.c_str());
        _canvas->drawCenterString(line, 120, 112);
    }
    _canvas->drawCenterString(message.c_str(), 120, 157);
    _canvas->drawCenterString("Press dial to rescan", 120, 195);
    _draw_quit_button();
    _canvas->pushSprite(0, 0);
}
