/**
 * @file more_menu_selected_callback.cpp
 * @author Forairaaaaa
 * @brief More menu selected callback
 * @version 0.1
 * @date 2023-08-10
 *
 * @copyright Copyright (c) 2023
 */

#include "app_more_menu.h"
#include "../common_define.h"
#include "../app_pc_control/app_pc_control.h"
#include "../app_dice/app_dice.h"
#include "../app_sound/app_sound.h"
#include "../app_rfid_test/app_rfid_test.h"
#include "../app_wifi_scan/app_wifi_scan.h"
#include "../app_ble_server/app_ble_server.h"
#include "../app_set_brightness/app_set_brightness.h"
#include "../app_rtc_test/app_rtc_test.h"
#include "../app_lcd_test/app_lcd_test.h"
#include "../app_virtual_dog/app_virtual_dog.h"
#include "../app_vault/app_vault.h"
#include "../app_wifi_scan/pc_control_config.h"
#include "../utilities/wifi_manager/wifi_manager.h"
#include <esp_http_client.h>
#include <cstring>
#include <cstdio>

#define USE_LVGL_DEMO

#ifdef USE_LVGL_DEMO
#include <hal/lvgl/hal_lvgl.hpp>
#include <hal/hal.h>
#include <lv_demos.h>
#endif

using namespace MOONCAKE::USER_APP;
using MOONCAKE::APP_BASE;

static void _run_sub_app(HAL::HAL* hal, MOONCAKE::APP_BASE* app, uint32_t color)
{
    if (app == nullptr || hal == nullptr) return;
    if (app->getGui() != nullptr) {
        LGFX_Sprite empty;
        app->getGui()->setThemeColor(color);
        app->getGui()->init(hal->canvas, &empty);
    }
    app->setUserData((void*)hal);
    app->onSetup();
    app->onCreate();
    while (1) {
        app->onRunning();
        if (app->isGoingDestroy()) {
            app->resetGoingDestroyFlag();
            app->onDestroy();
            break;
        }
    }
    delete app;
    hal->canvas->setTextDatum(textdatum_t::top_left);
    hal->canvas->fillScreen(TFT_BLACK);
}

void MoreMenu::_item_selected_callback(uint8_t selectedNum)
{
    std::string selected_item_tag =
        _data.menu->getMenu()->getItemList()[selectedNum]->tag.c_str();

    _log("selected: %s", selected_item_tag.c_str());

    if (selected_item_tag == "Secret Msg")
    {
        _run_sub_app(_data.hal, new MOONCAKE::USER_APP::VaultApp, 0xFF1493);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "Virtual Dog")
    {
        _run_sub_app(_data.hal, new MOONCAKE::USER_APP::VirtualDogApp, 0xFFA726);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "Drawing Pad")
    {
        _run_sub_app(_data.hal, new MOONCAKE::USER_APP::LCD_Test, 0xFD5C4C);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "Sound Settings")
    {
        MOONCAKE::USER_APP::run_sound_app(_data.hal);
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "Toggle Sound")
    {
        _data.hal->buzz.toggle();
        if (_data.hal->buzz.isEnabled()) {
            _data.hal->buzz.tone(3600, 40);
        }
        return;
    }

    if (selected_item_tag == "RFID Scanner")
    {
        _run_sub_app(_data.hal, new MOONCAKE::USER_APP::RFID_Test, 0x03A964);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "Wi-Fi Scanner")
    {
        _run_sub_app(_data.hal, new MOONCAKE::USER_APP::WiFi_Scan, 0xEB8429);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "BLE Server")
    {
        _run_sub_app(_data.hal, new MOONCAKE::USER_APP::BLE_Server, 0x04A279);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "RTC Clock")
    {
        _run_sub_app(_data.hal, new MOONCAKE::USER_APP::RTC_Test, 0x577EFF);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "Brightness Set")
    {
        _run_sub_app(_data.hal, new MOONCAKE::USER_APP::Set_Brightness, 0x1AA198);
        _canvas_update();
        return;
    }

    if (selected_item_tag == "Quit")
    {
        destroyApp();
        return;
    }

    if (selected_item_tag == "Power Off")
    {
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _canvas_update();

        _data.hal->rtc.clearIRQ();
        _data.hal->rtc.disableIRQ();

        delay(500);

        _data.hal->powerOff();

        delay(4000);
        _data.hal->powerOn();
    }

    if (selected_item_tag == "Dice Roller")
    {
        MOONCAKE::USER_APP::run_dice_app(_data.hal);
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _canvas_update();
        return;
    }

    // PC_CONTROL_CENTER_SMARTWATCH_CARD_DECK
    if (selected_item_tag == "PC Control Center")
    {
        MOONCAKE::USER_APP::run_pc_control_center(_data.hal);
        _data.hal->canvas->setTextDatum(textdatum_t::top_left);
        _data.hal->canvas->fillScreen(TFT_BLACK);
        _canvas_update();
        return;
    }

    /* LVGL demos */
    if (selected_item_tag.find("LVGL") != std::string::npos)
    {
        _callback_lvgl(selected_item_tag);
    }
}

void MoreMenu::_callback_lvgl(const std::string& selectedTag)
{
#ifdef USE_LVGL_DEMO

    /* Initialize LVGL */
    _log_mem();

    LVGL_PORT lvgl;
    lvgl.init(
        &_data.hal->display,
        &_data.hal->encoder,
        &_data.hal->tp,
        _data.hal->canvas->getBuffer()
    );

    _log_mem();

    /*
     * Start the selected LVGL demo only when its function is
     * declared and its demo implementation is enabled.
     *
     * The demo functions are unavailable in the current build,
     * so these calls are intentionally disabled.
     */

    if (selectedTag == "LVGL Widgets")
    {
        // lv_demo_widgets();  // Disabled: unavailable in current build
    }
    else if (selectedTag == "LVGL Stress")
    {
        // lv_demo_stress();  // Disabled: unavailable in current build
    }
    else if (selectedTag == "LVGL Benchmark")
    {
        // lv_demo_benchmark();  // Disabled: unavailable in current build
    }

    uint32_t button_time_count = millis();

    _data.hal->encoder.btn.setPin(42);
    _data.hal->encoder.btn.begin();

    /* LVGL loop */
    while (1)
    {
        lv_timer_handler();
        delay(5);

        /* Exit when the encoder button is pressed */
        if ((millis() - button_time_count) > 100)
        {
            if (!_data.hal->encoder.btn.read())
            {
                while (!_data.hal->encoder.btn.read())
                {
                    delay(10);
                }

                break;
            }

            button_time_count = millis();
        }
    }

    /* Display exit message */
    _data.hal->canvas->fillScreen(TFT_BLACK);
    _data.hal->canvas->setTextColor((uint32_t)0xF3E9D2);
    _data.hal->canvas->drawCenterString("QUITTING LVGL", 120, 120 - 12);
    _data.hal->canvas->pushSprite(0, 0);

    delay(800);

    _data.hal->canvas->fillScreen(TFT_BLACK);
    _data.hal->canvas->pushSprite(0, 0);

    /* Restart the device */
    esp_restart();

#endif
}
