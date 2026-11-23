/**
 * @file app_timer.h
 * @brief Countdown Timer with Buzzer Alarm for M5Dial
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
        namespace TIMER
        {
            enum State_t { IDLE, RUNNING, PAUSED, FINISHED };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                State_t state = IDLE;
                int32_t set_seconds = 180; // default 3 min
                int64_t end_us = 0;
                int64_t remain_us = 0;
                uint32_t last_render = 0;
                uint32_t last_alarm_beep = 0;
                bool alarm_flash = false;
            };
        }

        class TimerApp : public APP_BASE
        {
            private:
                const char* _tag = "timer";
                TIMER::Data_t _data;

                int64_t _remaining_us();
                void _start_pause();
                void _reset();
                void _render();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_timer_app(HAL::HAL* hal);
    }
}
