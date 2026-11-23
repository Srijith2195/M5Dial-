/**
 * @file app_omnitrix.h
 * @brief Ben 10 Classic Omnitrix Simulator for M5Dial
 * @version 0.1
 * @date 2026-11-18
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"

namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace OMNITRIX
        {
            enum Mode_t
            {
                MODE_STANDBY,     // Classic Black & Green Hourglass Face
                MODE_SELECTION,   // Dial Popped Up - Alien Selection Mode
                MODE_TRANSFORMED, // Dial Slammed Down - Alien Transformed Mode
                MODE_TIMEOUT      // Red Flashing Recharge / Timeout Mode
            };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                Mode_t mode = MODE_STANDBY;
                int selected_alien = 0; // 0..17
                uint32_t mode_start_time = 0;
                uint32_t last_pulse_time = 0;
                int pulse_bright = 0;
                bool pulse_up = true;
                uint32_t timeout_beep_time = 0;
                bool timeout_flash = false;
            };
        }

        class OmnitrixApp : public APP_BASE
        {
            private:
                const char* _tag = "omnitrix";
                OMNITRIX::Data_t _data;

                void _render_standby();
                void _render_selection();
                void _render_transformed();
                void _render_timeout();

                void _popup_dial();
                void _slam_dial();
                void _trigger_timeout();
                void _reset_standby();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_omnitrix_app(HAL::HAL* hal);
    }
}
