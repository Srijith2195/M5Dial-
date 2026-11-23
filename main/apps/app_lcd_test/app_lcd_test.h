/**
 * @file app_lcd_test.h
 * @brief Smooth Freehand Drawing Pad for M5Dial
 * @version 0.2
 * @date 2026-11-17
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"

namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace LCD_TEST
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                int color_index = 0;
                uint32_t badge_until = 0;
                uint32_t hint_until = 0;
            };
        }

        class LCD_Test : public APP_BASE
        {
            private:
                const char* _tag = "draw";

                LCD_TEST::Data_t _data;

                void _draw_stroke(int x0, int y0, int x1, int y1, uint32_t color, int radius);
                void _clear_canvas();
                void _show_color_badge();
                void _restore_badge_area();
                void _clear_hint();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };
    }
}
