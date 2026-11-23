/**
 * @file app_pc_control.h
 * @brief Smartwatch-grade PC Control Center App for M5Dial
 */
#pragma once
#include "../app.h"
#include <hal/hal.h>

namespace MOONCAKE {
    namespace USER_APP {
        class PC_Control : public APP_BASE {
            private:
                struct Data_t {
                    HAL::HAL* hal = nullptr;
                    int current_card = 0;
                    int local_volume = 50;
                    int local_brightness = 50;
                    bool local_muted = false;
                    std::string toast_msg = "";
                    uint32_t toast_until = 0;
                    uint32_t last_wifi_refresh = 0;
                    uint32_t last_status_fetch = 0;
                    bool has_synced_status = false;
                };
                Data_t _data;

                int _send_command(const char* cmd, int val = 0);
                int _fetch_status();
                void _parse_status_json(const std::string& json_str);
                void _render();
                void _handle_touch();
                void _handle_encoder();

            public:
                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_pc_control_center(HAL::HAL* hal);
    }
}
