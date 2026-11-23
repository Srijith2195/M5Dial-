/**
 * @file app_flashlight.h
 * @brief Multi-color Torch & Dimmer for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"

namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace FLASHLIGHT
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                int color_mode = 0; // 0: White, 1: Warm, 2: Red
                int brightness = 100; // 5..100
                int old_brightness = 100;
            };
        }

        class FlashlightApp : public APP_BASE
        {
            private:
                const char* _tag = "flashlight";
                FLASHLIGHT::Data_t _data;

                void _apply();
                void _next_color();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_flashlight_app(HAL::HAL* hal);
    }
}
