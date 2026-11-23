/**
 * @file app_more_menu.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "app_more_menu.h"
#include "../common_define.h"


using namespace MOONCAKE::USER_APP;


void MoreMenu::_create_menu()
{
    /* Create a menu to handle selector */
    _data.menu = new SMOOTH_MENU::Simple_Menu;
    _data.menu_render_cb = new MoreMenuRender_CB_t;
    _data.menu_render_cb->setCanvas(_data.hal->canvas);

    _data.menu->init(240, 240);
    _data.menu->setRenderCallback(_data.menu_render_cb);

    /* Set selector anim */
    auto cfg_selector = _data.menu->getSelector()->config();
    cfg_selector.animPath_x = LVGL::overshoot;
    cfg_selector.animPath_y = LVGL::overshoot;
    cfg_selector.animTime_x = 400;
    cfg_selector.animTime_y = 400;
    _data.menu->getSelector()->config(cfg_selector);

    /* Set menu loop mode */
    _data.menu->setMenuLoopMode(false);


    /* Add menu item tag */
    int item_h = 32;
    int item_w = 176;
    int item_x = (240 - item_w) / 2;
    std::vector<std::string> tag_list = {
        "Secret Msg",
        "Virtual Dog",
        "Sound Settings",
        "Toggle Sound",
        "RFID Scanner",
        "Wi-Fi Scanner",
        "BLE Server",
        "RTC Clock",
        "Brightness Set",
        "LVGL Widgets",
        "LVGL Benchmark",
        "Power Off",
        "Quit"
    };


    /* Push item into menu */
    for (int i = 0; i < tag_list.size(); i++)
    {
        _data.menu->getMenu()->addItem(
            tag_list[i].c_str(),
            item_x,
            10 + item_h * i,
            item_w,
            item_h
        );
    }
}


void MoreMenu::_menu_loop()
{
    _data.menu->update(millis());
    _canvas_update();
    delay(10);

    /* If scrolled */
    if (_data.hal->encoder.wasMoved(true))
    {
        if (_data.hal->encoder.getDirection() < 1)
            _data.menu->goNext();
        else 
            _data.menu->goLast();
    }

    /* If button pressed */
    if (!_data.hal->encoder.btn.read())
    {
        _data.menu->getSelector()->pressed();
        while (!_data.hal->encoder.btn.read())
        {
            _data.menu->update(millis());
            _canvas_update();
            delay(10);
        }

        _data.menu->getSelector()->released();
        while (!_data.menu->getSelector()->isAnimFinished())
        {
            _data.menu->update(millis());
            _canvas_update();
            delay(10);
        }

        /* App open callback */
        _item_selected_callback(_data.menu->getSelector()->getTargetItem());
        return;
    }

    /* Smartwatch Touch Navigation in MoreMenu */
    if (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        auto pt = _data.hal->tp.getTouchPointBuffer();
        int start_x = pt.x;
        int start_y = pt.y;
        int last_x = pt.x;
        int last_y = pt.y;
        uint32_t start_time = millis();

        while (_data.hal->tp.isTouched())
        {
            _data.hal->tp.update();
            last_x = _data.hal->tp.getTouchPointBuffer().x;
            last_y = _data.hal->tp.getTouchPointBuffer().y;
            _data.menu->update(millis());
            _canvas_update();
            delay(10);
        }

        int dx = last_x - start_x;
        int dy = last_y - start_y;
        uint32_t duration = millis() - start_time;

        // Swipe Left -> Navigate Back to Launcher
        if (dx < -40 && abs(dy) < 55)
        {
            _data.hal->buzz.tone(3000, 30);
            destroyApp();
            return;
        }

        // 2. Swipe Up / Down -> Smooth Scroll
        if (dy < -35 && abs(dx) < 50)
        {
            _data.hal->buzz.tone(3600, 10);
            _data.menu->goNext();
        }
        else if (dy > 35 && abs(dx) < 50)
        {
            _data.hal->buzz.tone(3600, 10);
            _data.menu->goLast();
        }
        // 3. Tap -> Direct Select
        else if (abs(dx) <= 20 && abs(dy) <= 20 && duration < 500)
        {
            _data.hal->buzz.tone(4000, 15);
            // If tapped top left corner, treat as Back
            if (start_x < 65 && start_y < 45)
            {
                destroyApp();
                return;
            }

            _item_selected_callback(_data.menu->getSelector()->getTargetItem());
            return;
        }
    }
}


void MoreMenu::onSetup()
{
    setAppName("MoreMenu");
    setAllowBgRunning(false);

    /* Copy default value */
    MORE_MENU::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


/* Life cycle */
void MoreMenu::onCreate()
{
    _log("onCreate");
    _create_menu();
}


void MoreMenu::onRunning()
{
    _menu_loop();

    // /* If touched */
    // if (_data.hal->tp.isTouched())
    // {
    //     _data.hal->tp.update();
    //     _log("%d %d", _data.hal->tp.getTouchPointBuffer().x, _data.hal->tp.getTouchPointBuffer().y);
    // }
}


void MoreMenu::onDestroy()
{
    _log("onDestroy");

    delete _data.menu;
    delete _data.menu_render_cb;
}
