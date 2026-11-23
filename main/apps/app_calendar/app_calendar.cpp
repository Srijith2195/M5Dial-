/**
 * @file app_calendar.cpp
 * @brief RTC Synchronized Month Calendar for M5Dial
 * @version 0.1
 * @date 2026-11-17
 */
#include "app_calendar.h"
#include "../common_define.h"
#include <cstdio>
#include <cstdlib>

using namespace MOONCAKE::USER_APP;

static const char* s_cal_months[] = {
    "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
    "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};

int CalendarApp::_days_in_month(int y, int m)
{
    static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 1 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) {
        return 29;
    }
    return days[m];
}

int CalendarApp::_first_weekday(int y, int m)
{
    struct tm t = {};
    t.tm_year = y - 1900;
    t.tm_mon = m;
    t.tm_mday = 1;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    mktime(&t);
    // Convert Sunday=0 to Monday=0
    return (t.tm_wday + 6) % 7;
}

void CalendarApp::_go_today()
{
    if (_data.hal != nullptr) {
        tm now = {};
        _data.hal->rtc.getTime(now);
        _data.today_year = now.tm_year + 1900;
        _data.today_month = now.tm_mon;
        _data.today_day = now.tm_mday;
        if (_data.today_year < 2020) _data.today_year = 2026;
        if (_data.today_day < 1) _data.today_day = 1;
    }
    _data.view_year = _data.today_year;
    _data.view_month = _data.today_month;
    _render();
}

void CalendarApp::_shift_month(int d)
{
    _data.view_month += d;
    while (_data.view_month < 0) {
        _data.view_month += 12;
        _data.view_year--;
    }
    while (_data.view_month > 11) {
        _data.view_month -= 12;
        _data.view_year++;
    }
    if (_data.hal != nullptr) {
        _data.hal->buzz.tone(3600, 15);
    }
    _render();
}

void CalendarApp::_render()
{
    if (_data.hal == nullptr) return;

    _data.hal->canvas->fillScreen(TFT_BLACK);

    // 1. Month Header Pill
    _data.hal->canvas->fillRoundRect(35, 10, 170, 24, 12, (uint32_t)0x1A1A1A);
    _data.hal->canvas->drawRoundRect(35, 10, 170, 24, 12, (uint32_t)0x333333);

    char hdr_buf[48];
    snprintf(hdr_buf, sizeof(hdr_buf), "%s %d", s_cal_months[_data.view_month], _data.view_year);
    _data.hal->canvas->setTextDatum(textdatum_t::middle_center);
    _data.hal->canvas->setTextColor((uint32_t)0x2979FF, (uint32_t)0x1A1A1A);
    _data.hal->canvas->setFont(&fonts::Font0);
    _data.hal->canvas->drawString(hdr_buf, 120, 22);

    // 2. Day-of-week labels (M T W T F S S)
    const int cw = 26;
    const int ch = 24;
    const int x0 = 120 - (7 * cw) / 2;
    const int y0 = 40;

    static const char* wd_labels[] = { "M", "T", "W", "T", "F", "S", "S" };
    for (int col = 0; col < 7; col++) {
        uint32_t col_txt = (col >= 5) ? (uint32_t)0xFF5252 : (uint32_t)0x888888;
        _data.hal->canvas->setTextColor(col_txt, TFT_BLACK);
        _data.hal->canvas->drawString(wd_labels[col], x0 + col * cw + cw / 2, y0 + 10);
    }

    // Divider line
    _data.hal->canvas->drawFastHLine(x0, y0 + 20, 7 * cw, (uint32_t)0x333333);

    // 3. Month grid
    int first_w = _first_weekday(_data.view_year, _data.view_month);
    int dim = _days_in_month(_data.view_year, _data.view_month);

    int start_y = y0 + 24;
    for (int i = 0; i < 42; i++) {
        int day = i - first_w + 1;
        if (day < 1 || day > dim) continue;

        int row = i / 7;
        int col = i % 7;
        int cx = x0 + col * cw + cw / 2;
        int cy = start_y + row * ch + ch / 2;

        bool is_today = (day == _data.today_day &&
                         _data.view_month == _data.today_month &&
                         _data.view_year == _data.today_year);
        bool is_weekend = (col >= 5);

        if (is_today) {
            _data.hal->canvas->fillCircle(cx, cy, 10, (uint32_t)0x2979FF);
            _data.hal->canvas->setTextColor(TFT_WHITE, (uint32_t)0x2979FF);
        } else if (is_weekend) {
            _data.hal->canvas->setTextColor((uint32_t)0xFF5252, TFT_BLACK);
        } else {
            _data.hal->canvas->setTextColor(TFT_WHITE, TFT_BLACK);
        }

        char d_str[16];
        snprintf(d_str, sizeof(d_str), "%d", day);
        _data.hal->canvas->drawString(d_str, cx, cy);
    }

    // 4. Bottom Hint
    _data.hal->canvas->setTextColor((uint32_t)0x555555, TFT_BLACK);
    _data.hal->canvas->drawString("Rotate: Month  |  Click: Today", 120, 218);

    _data.hal->canvas->pushSprite(0, 0);
}

void CalendarApp::onSetup()
{
    setAppName("Calendar");
    setAllowBgRunning(false);

    CALENDAR::Data_t default_data;
    _data = default_data;
    _data.hal = (HAL::HAL*)getUserData();
}

void CalendarApp::onCreate()
{
    _log("onCreate");
    _go_today();
}

void CalendarApp::onRunning()
{
    // 1. Dial knob rotation: Shift month
    if (_data.hal->encoder.wasMoved(true))
    {
        if (_data.hal->encoder.getDirection() < 1) {
            _shift_month(1);
        } else {
            _shift_month(-1);
        }
        delay(80);
    }

    // 2. Dial knob click: Return to today / long press: exit
    bool btn_down = (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0);
    if (btn_down)
    {
        uint32_t press_start = millis();
        while (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0)
        {
            delay(10);
            if (millis() - press_start > 650)
            {
                _data.hal->buzz.tone(2600, 30);
                while (!_data.hal->encoder.btn.read() || gpio_get_level((gpio_num_t)HAL_PIN_PWR_WAKE_UP) == 0) delay(10);
                destroyApp();
                return;
            }
        }
        _data.hal->buzz.tone(3600, 20);
        _go_today();
    }

    // 3. Touch Screen
    if (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        auto pt = _data.hal->tp.getTouchPointBuffer();
        int start_x = pt.x, start_y = pt.y, last_x = pt.x, last_y = pt.y;
        uint32_t start_time = millis();

        while (_data.hal->tp.isTouched())
        {
            _data.hal->tp.update();
            last_x = _data.hal->tp.getTouchPointBuffer().x;
            last_y = _data.hal->tp.getTouchPointBuffer().y;
            delay(10);
        }

        int dx = last_x - start_x;
        int dy = last_y - start_y;
        uint32_t duration = millis() - start_time;

        // Swipe Left -> Exit to Launcher
        if (dx < -40 && abs(dy) < 55)
        {
            _data.hal->buzz.tone(3000, 30);
            destroyApp();
            return;
        }

        // Swipe Right -> Next Month
        if (dx > 40 && abs(dy) < 55)
        {
            _shift_month(1);
            return;
        }

        // Tap top header -> Today
        if (start_y < 38 && abs(dx) < 25 && duration < 600)
        {
            _go_today();
        }
    }

    delay(20);
}

void CalendarApp::onDestroy()
{
    _log("onDestroy");
    if (_data.hal != nullptr)
    {
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _data.hal->canvas->pushSprite(0, 0);
    }
}

void MOONCAKE::USER_APP::run_calendar_app(HAL::HAL* hal)
{
    CalendarApp app;
    app.setUserData(hal);
    app.onSetup();
    app.onCreate();
    while (1) {
        app.onRunning();
        if (app.isGoingDestroy()) {
            app.resetGoingDestroyFlag();
            app.onDestroy();
            break;
        }
    }
}
