/**
 * @file app_rtc_test.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date 2023-08-03
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "app_rtc_test.h"
#include "../common_define.h"

using namespace MOONCAKE::USER_APP;

void RTC_Test::onSetup()
{
    // setAppName("RTC_Test");
    setAllowBgRunning(false);

    /* Copy default value */
    RTC_TEST::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}

/* Life cycle */
void RTC_Test::onCreate() { _log("onCreate"); }

void RTC_Test::onRunning()
{
    /* Smartwatch Touch Handling in RTC_Test */
    if (_data.hal != nullptr && _data.hal->tp.isTouched()) {
        _data.hal->tp.update();
        auto pt = _data.hal->tp.getTouchPointBuffer();
        int start_x = pt.x;
        int start_y = pt.y;
        int last_x = pt.x;
        int last_y = pt.y;

        while (_data.hal->tp.isTouched()) {
            _data.hal->tp.update();
            last_x = _data.hal->tp.getTouchPointBuffer().x;
            last_y = _data.hal->tp.getTouchPointBuffer().y;
            delay(10);
        }

        int dx = last_x - start_x;
        int dy = last_y - start_y;

        // Top-Left "< Back" or Universal Swipe Right or Bottom Tap
        if ((start_x < 75 && start_y < 42) || (dx > 45 && abs(dy) < 55) || (start_y > 200 && abs(dx) < 50)) {
            _data.hal->buzz.tone(3000, 20);
            destroyApp();
            return;
        }
    }

    /* If button pressed */
    if (!_data.hal->encoder.btn.read())
    {
        /* Hold until button release */
        while (!_data.hal->encoder.btn.read())
            delay(5);

        /* Bye */
        destroyApp();
    }

    /* Update page */
    if ((millis() - _data.time_count) > 1000)
    {
        _data.hal->rtc.getTime(_data.rtc_time);
        _log("%d:%d:%d", _data.rtc_time.tm_hour, _data.rtc_time.tm_min, _data.rtc_time.tm_sec);

        _gui.renderPage(_data.rtc_time.tm_hour, _data.rtc_time.tm_min, _data.rtc_time.tm_sec);

        _data.time_count = millis();
    }
}

void RTC_Test::onDestroy() { _log("onDestroy"); }
