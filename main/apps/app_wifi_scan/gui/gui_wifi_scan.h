#pragma once
#include <string>
#include <vector>
#include "../../utilities/gui_base/gui_base.h"
#include "../../utilities/wifi_manager/wifi_manager.h"

class GUI_WiFi_Scan : public GUI_Base {
public:
    void init() override;
    void renderNetworks(const std::vector<WIFI_MANAGER::Network>& networks, int selected, const std::string& message);
    void renderPassword(const std::string& ssid, const std::string& password, int selectedChar, const std::string& message);
    void renderStatus(const std::string& ssid, const std::string& status, const std::string& ip, const std::string& message);
};
