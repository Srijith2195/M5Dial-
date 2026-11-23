/**
 * @file app_calendar.h
 * @brief RTC Synchronized Month Calendar for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include <ctime>

namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace CALENDAR
        {
            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                int view_year = 2026;
                int view_month = 10; // 0..11
                int today_year = 2026;
                int today_month = 10;
                int today_day = 17;
            };
        }

        class CalendarApp : public APP_BASE
        {
            private:
                const char* _tag = "calendar";
                CALENDAR::Data_t _data;

                int _days_in_month(int y, int m);
                int _first_weekday(int y, int m); // 0 = Mon
                void _go_today();
                void _shift_month(int d);
                void _render();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_calendar_app(HAL::HAL* hal);
    }
}
