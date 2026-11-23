/**
 * @file launcher.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-07-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "launcher.h"
#include "../common_define.h"


using namespace MOONCAKE::USER_APP;


void Launcher::_menu_init()
{
    /* Create a menu to handle selector */
    _data.menu = new SMOOTH_MENU::Simple_Menu;
    _data.menu_render_cb = new LauncherRender_CB_t;
    _data.menu_render_cb->setCanvas(_data.hal->canvas);

    _data.menu->init(240, 240);
    _data.menu->setRenderCallback(_data.menu_render_cb);

    /* Set selector anim */
    auto cfg_selector = _data.menu->getSelector()->config();
    cfg_selector.animPath_x = LVGL::overshoot;
    cfg_selector.animPath_y = LVGL::overshoot;
    cfg_selector.animTime_x = 300;
    cfg_selector.animTime_y = 300;
    _data.menu->getSelector()->config(cfg_selector);

    /* Set menu looply */
    _data.menu->setMenuLoopMode(true);
    _data.menu->getMenu()->x = 0;
    _data.menu->getMenu()->y = 0;

    /* Push selector points into menu */
    int a = 120;
    int b = 120;
    int r = 60;
    int n = ICON_NUM;
    int x;
    int y;
    for (int i = 0; i < n; i++)
    {
        x = a + r * std::cos(2 * 3.14 * i / n);
        y = b + r * std::sin(2 * 3.14 * i / n);
        _data.menu->getMenu()->addItem("", x, y, ICON_RADIUS, ICON_RADIUS);
    }
}


void Launcher::_icon_list_init()
{
    for (int i = 0; i < icon_list.size(); i++)
    {
        /* Set colors */
        icon_list[i].color = icon_color_list[i];

        /* Set tags */
        icon_list[i].tag_up = icon_tag_list[i * 2];
        icon_list[i].tag_down = icon_tag_list[i * 2 + 1];

        /* Push Icon pic into sprite */
        icon_sprite_list[i].createSprite(42, 42);
        icon_sprite_list[i].pushImage(0, 0, 42, 42, icon_pic_list[i]);
    }

    /* Icon position */
    int a = 120;
    int b = 120;
    int r = 190 / 2;
    int n = ICON_NUM;
    int x;
    int y;
    for (int i = 0; i < icon_list.size(); i++)
    {
        x = a + r * std::cos(2 * 3.14 * i / n);
        y = b + r * std::sin(2 * 3.14 * i / n);

        icon_list[i].x = x;
        icon_list[i].y = y;
    }
}


void Launcher::_launcher_init()
{
    _menu_init();
    _icon_list_init();
}


void Launcher::_launcher_loop()
{
    _data.menu->update(millis());
    _canvas_update();
    // delay(5);

    /* If scrolled */
    if (_data.hal->encoder.wasMoved(true))
    {
        _data.hal->buzz.tone(3600, 12);
        if (_data.hal->encoder.getDirection() < 1)
            _data.menu->goNext();
        else 
            _data.menu->goLast();
    }

    /* If button pressed */
    if (!_data.hal->encoder.btn.read())
    {
        _data.menu->getSelector()->pressed();

        /* Hold until button release */
        while (!_data.hal->encoder.btn.read())
        {
            _data.menu->update(millis());
            _canvas_update();
        }

        _data.menu->getSelector()->released();

        // /* Hold until anim finish */
        // while (!_data.menu->getSelector()->isAnimFinished())
        // {
        //     _data.menu->update(millis());
        //     _canvas_update();
        // }

        /* App open callback */
        _app_open_callback(_data.menu->getSelector()->getTargetItem());
    }

    /* Smartwatch Touch Interaction in Launcher */
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

        // 1. Swipe Left / Right to rotate app dial
        if (dx < -40 && abs(dy) < 55)
        {
            _data.hal->buzz.tone(3600, 10);
            _data.menu->goNext();
            return;
        }
        else if (dx > 40 && abs(dy) < 55)
        {
            _data.hal->buzz.tone(3600, 10);
            _data.menu->goLast();
            return;
        }

        // 2. Tap detection
        if (abs(dx) <= 25 && abs(dy) <= 25 && duration < 600)
        {
            // Check if user tapped directly on one of the circular app icons around the rim
            int closest_icon = -1;
            int min_dist_sq = 32 * 32;
            for (int i = 0; i < icon_list.size(); i++)
            {
                int idx_dx = start_x - icon_list[i].x;
                int idx_dy = start_y - icon_list[i].y;
                int dist_sq = idx_dx * idx_dx + idx_dy * idx_dy;
                if (dist_sq < min_dist_sq)
                {
                    min_dist_sq = dist_sq;
                    closest_icon = i;
                }
            }

            if (closest_icon >= 0)
            {
                _data.hal->buzz.tone(4200, 15);
                _data.menu->getSelector()->goToItem(closest_icon);
                _app_open_callback(closest_icon);
                return;
            }

            // Check if tapped inside center circle (r <= 50)
            int cx = start_x - 120;
            int cy = start_y - 120;
            if ((cx * cx + cy * cy) <= (50 * 50))
            {
                _data.hal->buzz.tone(4200, 15);
                HAL::HAL::_encoder_button_pressed_callback(nullptr, _data.hal);
                _app_open_callback(_data.menu->getSelector()->getTargetItem());
                return;
            }
        }
    }
}


void Launcher::_app_open_callback(uint8_t selectedNum)
{
    _log("selected %d", selectedNum);

    /* If not in icon list */
    if (selectedNum >= icon_list.size())
    {
        return;
    }


    /* Special color for app more menu */
    uint32_t theme_color = 0;
    if (selectedNum != (ICON_NUM - 1))
        theme_color = icon_list[selectedNum].color;
    else
        theme_color = 0;


    /* Play app open anim */
    for (int i = 0; i < 10; i++)
    {
        _data.hal->canvas->fillSmoothCircle(icon_list[selectedNum].x, icon_list[selectedNum].y, i * 24, theme_color);
        _canvas_update();
    }

    

    // /* ----------------------- Simple test ----------------------- */
    // std::array<GUI_Base*, ICON_NUM> gui_list;
    // gui_list[0] = new GUI_Base;
    // gui_list[1] = new GUI_Base;
    // gui_list[2] = new GUI_Base;
    // gui_list[3] = new GUI_Base;
    // gui_list[4] = new GUI_Base;
    // gui_list[5] = new GUI_Base;
    // gui_list[6] = new GUI_Base;
    // gui_list[7] = new GUI_Base;

    // /* Open app */
    // gui_list[selectedNum]->setThemeColor(icon_list[selectedNum].color);
    // gui_list[selectedNum]->init(_data.hal->canvas, &icon_sprite_list[selectedNum]);
    // while (1)
    // {
    //     if (_data.hal->encoder.btn.pressed())
    //     {
    //         /* Hold until button release */
    //         while (!_data.hal->encoder.btn.read());
    //         break;
    //     }
    // }
    // /* ----------------------- Simple test ----------------------- */






    // /* ----------------------- Simple app test ----------------------- */
    // std::array<MOONCAKE::APP_BASE*, ICON_NUM> app_list;
    // app_list[0] = new MOONCAKE::USER_APP::LCD_Test;
    // app_list[1] = new MOONCAKE::USER_APP::RTC_Test;
    // app_list[2] = new MOONCAKE::USER_APP::RFID_Test;
    // app_list[3] = new MOONCAKE::USER_APP::Set_Brightness;
    // app_list[4] = new MOONCAKE::USER_APP::WiFi_Scan;
    // app_list[5] = new MOONCAKE::APP_BASE;
    // app_list[6] = new MOONCAKE::APP_BASE;
    // app_list[7] = new MOONCAKE::USER_APP::VideoShit;

    // if (app_list[selectedNum]->getGui() != nullptr)
    // {
    //     app_list[selectedNum]->getGui()->setThemeColor(icon_list[selectedNum].color);
    //     app_list[selectedNum]->getGui()->init(_data.hal->canvas, &icon_sprite_list[selectedNum]);

    //     _simple_app_manager(app_list[selectedNum]);
    // }

    // /* Free */
    // for (auto& i : app_list)
    // {
    //     delete i;
    // }
    // /* ----------------------- Simple app test ----------------------- */




    /* Memery leak check */
    size_t mem_before_open = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    _log("free mem: %d", mem_before_open);



    MOONCAKE::APP_BASE* app_ptr = nullptr;

    /* Create app */
    switch (selectedNum)
    {
        case 0:
            app_ptr = new MOONCAKE::USER_APP::OmnitrixApp;
            break;
        case 1:
            app_ptr = new MOONCAKE::USER_APP::PC_Control;
            break;
        case 2:
            app_ptr = new MOONCAKE::USER_APP::StopwatchApp;
            break;
        case 3:
            app_ptr = new MOONCAKE::USER_APP::TimerApp;
            break;
        case 4:
            app_ptr = new MOONCAKE::USER_APP::FlashlightApp;
            break;
        case 5:
            app_ptr = new MOONCAKE::USER_APP::LCD_Test;
            break;
        case 6:
            app_ptr = new MOONCAKE::USER_APP::BreakoutApp;
            break;
        case 7:
            app_ptr = new MOONCAKE::USER_APP::Dice;
            break;
        case 8:
            app_ptr = new MOONCAKE::USER_APP::CoinFlipApp;
            break;
        case 9:
            app_ptr = new MOONCAKE::USER_APP::VirtualDogApp;
            break;
        case 10:
            app_ptr = new MOONCAKE::USER_APP::VaultApp;
            break;
        case 11:
            app_ptr = new MOONCAKE::USER_APP::MoreMenu;
            break;
        default:
            break;
    };

    /* If app created */
    if (app_ptr != nullptr)
    {
        /* Init if gui module exsit */
        if (app_ptr->getGui() != nullptr)
        {
            app_ptr->getGui()->setThemeColor(icon_list[selectedNum].color);
            app_ptr->getGui()->init(_data.hal->canvas, &icon_sprite_list[selectedNum]);
        }

        /* Run app */
        _simple_app_manager(app_ptr);
        
        /* Free app */
        delete app_ptr;
    }



    /* Memery leak check */
    _log_mem();
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < mem_before_open)
    {
        _log_e("memory leak: %d", mem_before_open - heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
    
    

    /* Play app close anim */
    for (int i = 10; i > 1; i--)
    {
        _data.menu->update(millis());
        _data.hal->canvas->fillSmoothCircle(icon_list[selectedNum].x, icon_list[selectedNum].y, i * 24, theme_color);
        _canvas_update();
    }
}


void Launcher::_simple_app_manager(MOONCAKE::APP_BASE* app)
{
    app->setUserData((void*)_data.hal);
    app->onSetup();
    app->onCreate();
    while (1)
    {
        app->onRunning();
        if (app->isGoingDestroy())
        {
            app->resetGoingDestroyFlag();
            app->onDestroy();
            break;
        }

        bool is_smart_app = (app != nullptr &&
            (app->getAppName() == "Draw Pad" ||
             app->getAppName() == "PC Control" ||
             app->getAppName() == "Dice" ||
             app->getAppName() == "Sound Settings" ||
             app->getAppName() == "Stopwatch" ||
             app->getAppName() == "Timer" ||
             app->getAppName() == "Flashlight" ||
             app->getAppName() == "Calendar" ||
             app->getAppName() == "Breakout" ||
             app->getAppName() == "Coin Flip" ||
             app->getAppName() == "Omnitrix" ||
             app->getAppName() == "Virtual Dog" ||
             app->getAppName() == "Secret Msg" ||
             app->getAppName() == "Vault" ||
             app->getAppName() == "MoreMenu"));

        /* Smartwatch Universal Exit Fallback for legacy apps: dial button long-press (>800ms) */
        if (!is_smart_app && !_data.hal->encoder.btn.read())
        {
            uint32_t press_start = millis();
            while (!_data.hal->encoder.btn.read())
            {
                delay(10);
                if (millis() - press_start > 800)
                {
                    _data.hal->buzz.tone(2800, 30);
                    while (!_data.hal->encoder.btn.read()) delay(10);
                    app->destroyApp();
                    break;
                }
            }
        }

        /* Smartwatch Universal Touch Navigation Fallback for apps that don't handle touch */
        if (!is_smart_app && _data.hal->tp.isTouched())
        {
            _data.hal->tp.update();
            auto pt = _data.hal->tp.getTouchPointBuffer();
            int s_x = pt.x, s_y = pt.y, l_x = pt.x, l_y = pt.y;
            uint32_t t0 = millis();
            while (_data.hal->tp.isTouched())
            {
                _data.hal->tp.update();
                l_x = _data.hal->tp.getTouchPointBuffer().x;
                l_y = _data.hal->tp.getTouchPointBuffer().y;
                delay(10);
            }
            int t_dx = l_x - s_x;
            int t_dy = l_y - s_y;

            // Universal Smartwatch Swipe Left ONLY -> Navigate Back to Launcher
            if (t_dx < -40 && abs(t_dy) < 55)
            {
                _data.hal->buzz.tone(3000, 30);
                app->destroyApp();
            }
        }
    }
}


Launcher::~Launcher()
{
    delete _data.menu;
    delete _data.menu_render_cb;
}


void Launcher::onSetup()
{
    setAppName("Launcher");
    setAllowBgRunning(false);

    /* Init with default value */
    LAUNCHER::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


/* Life cycle */
void Launcher::onCreate()
{
    _log("onCreate");
    
    _launcher_init();
    _data.menu->getSelector()->reset(millis());
}


void Launcher::onRunning()
{
    _launcher_loop();
}

