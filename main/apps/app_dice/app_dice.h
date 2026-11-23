/**
 * @file app_dice.h
 * @brief Smartwatch Two-Dice Roller App for M5Dial
 */
#pragma once
#include "../app.h"
#include <hal/hal.h>

namespace MOONCAKE {
    namespace USER_APP {
        class Dice : public APP_BASE {
            private:
                struct Data_t {
                    HAL::HAL* hal = nullptr;
                    int value[2] = {6, 6};
                    bool is_rolling = false;
                    int roll_frames = 0;
                    uint32_t last_frame_time = 0;
                    int angle[2] = {0, 0};
                };
                Data_t _data;

                void _roll();
                void _render();
                void _draw_die(int cx, int cy, int val, uint32_t pip_color, int angle);

            public:
                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_dice_app(HAL::HAL* hal);
    }
}
