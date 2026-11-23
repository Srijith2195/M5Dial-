/**
 * @file app_sound.h
 * @brief Sound Configuration App for M5Dial
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
        namespace SOUND_CONFIG
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                bool sound_enabled = true;
            };
        }

        class SoundApp : public APP_BASE
        {
            private:
                const char* _tag = "sound";
                SOUND_CONFIG::Data_t _data;

                void _render();
                void _toggle_sound();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_sound_app(HAL::HAL* hal);
    }
}
