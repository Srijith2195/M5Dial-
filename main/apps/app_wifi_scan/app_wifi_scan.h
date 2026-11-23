
#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "../app.h"
#include "../../hal/hal.h"
#include "gui/gui_wifi_scan.h"
#include "../utilities/smooth_menu/src/lv_anim/lv_anim.h"
#include "../utilities/wifi_manager/wifi_manager.h"

namespace MOONCAKE {
namespace USER_APP {
namespace WIFI_SCAN {

enum class Page {
    NETWORKS,
    PASSWORD,
    STATUS
};

struct Data_t {
    HAL::HAL* hal = nullptr;

    int delta_time = 0;
    int offset_increment = 0;
    uint32_t scroll_speed_time_count = 0;
    int menu_y_offset = 0;

    std::string wifi_scan_result;
    std::vector<WIFI_MANAGER::Network> networks;

    int selected_network = 0;
    int selected_char = 0;

    std::string password;
    std::string status_message;

    Page page = Page::NETWORKS;

    // Encoder button edge detection.
    bool button_was_down = false;

    LVGL::Anim_Path menu_y_offset_anim;
};

} // namespace WIFI_SCAN

class WiFi_Scan : public APP_BASE {
private:
    const char* _tag = "wifi";

    void _scan_wifi();
    void _render();
    void _handle_encoder();
    void _connect_selected();

public:
    WIFI_SCAN::Data_t _data;
    GUI_WiFi_Scan _gui;

    GUI_Base* getGui() override {
        return &_gui;
    }

    void onSetup();
    void onCreate();
    void onRunning();
    void onDestroy();
};

} // namespace USER_APP
} // namespace MOONCAKE
 