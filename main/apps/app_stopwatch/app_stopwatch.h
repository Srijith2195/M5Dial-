/**
 * @file app_stopwatch.h
 * @brief Precision Lap Stopwatch for M5Dial
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
        namespace STOPWATCH
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                bool running = false;
                int64_t start_us = 0;
                int64_t acc_us = 0;
                int64_t laps[12];
                int lap_count = 0;
                uint32_t last_render = 0;
            };
        }

        class StopwatchApp : public APP_BASE
        {
            private:
                const char* _tag = "stopwatch";
                STOPWATCH::Data_t _data;

                int64_t _elapsed_us();
                void _format_time(char* buf, size_t max_len, int64_t us);
                void _start_stop();
                void _lap_reset();
                void _render();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_stopwatch_app(HAL::HAL* hal);
    }
}
