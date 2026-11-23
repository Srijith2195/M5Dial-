/**
 * @file more_menu_render_callback.hpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once


/* Render setting */
#define MORE_MENU_COLOR_SELECTOR           (uint32_t)0xfa7000
#define MORE_MENU_COLOR_BG                  TFT_BLACK


struct MoreMenuRender_CB_t : public SMOOTH_MENU::SimpleMenuCallback_t
{
    private:
        LGFX_Sprite* _canvas;

    public:
        inline void setCanvas(LGFX_Sprite* canvasPtr) { _canvas = canvasPtr; }

        /* Override render callback */
        void renderCallback(
            const std::vector<SMOOTH_MENU::Item_t*>& menuItemList,
            const SMOOTH_MENU::RenderAttribute_t& selector,
            const SMOOTH_MENU::RenderAttribute_t& camera
        ) override
        {
            _canvas->fillScreen(MORE_MENU_COLOR_BG);
            _canvas->setTextSize(1);

            // Subtle dial border ring
            _canvas->drawCircle(120, 120, 118, (uint32_t)0x3A1800);
            _canvas->drawCircle(120, 120, 117, (uint32_t)0x221000);

            // Top Header
            _canvas->setTextDatum(textdatum_t::middle_center);
            _canvas->setTextColor(MORE_MENU_COLOR_SELECTOR);
            _canvas->setFont(&fonts::Font0);
            _canvas->drawString("~ MORE APPS ~", 120, 16);

            // Center Selector Pill (Anchored at vertical & horizontal center)
            int pill_w = 176;
            int pill_h = 30;
            int pill_x = (240 - pill_w) / 2;
            int pill_y = 120 - pill_h / 2;
            _canvas->fillSmoothRoundRect(pill_x, pill_y, pill_w, pill_h, 8, MORE_MENU_COLOR_SELECTOR);
            _canvas->drawRoundRect(pill_x, pill_y, pill_w, pill_h, 8, (uint32_t)0xFFD180);

            // Render all items centered on the round dial
            for (size_t i = 0; i < menuItemList.size(); i++)
            {
                // Calculate item center y relative to animated selector.y
                int item_center_y = 120 + (menuItemList[i]->y - selector.y);
                int dist_from_center = std::abs(item_center_y - 120);

                // Cull items outside circular screen boundary
                if (dist_from_center > 92) continue;

                // Choose font based on string length
                if (menuItemList[i]->tag.length() > 12)
                {
                    _canvas->setFont(GUI_FONT_CN_SMALL);
                }
                else
                {
                    _canvas->setFont(GUI_FONT_CN_BIG);
                }
                _canvas->setTextSize(1);
                _canvas->setTextDatum(textdatum_t::middle_center);

                if (dist_from_center < 15)
                {
                    // Item inside the selector pill: Bold black text
                    _canvas->setTextColor(TFT_BLACK);
                    _canvas->drawString(menuItemList[i]->tag.c_str(), 120, item_center_y);
                }
                else
                {
                    // Unselected items: Fade out with distance from center
                    int alpha = 240 - dist_from_center * 2.2f;
                    if (alpha < 40) alpha = 40;
                    _canvas->setTextColor(lgfx::color565(alpha, alpha, alpha));
                    _canvas->drawString(menuItemList[i]->tag.c_str(), 120, item_center_y);
                }
            }

            // Bottom Footer Hint
            _canvas->setTextDatum(textdatum_t::middle_center);
            _canvas->setTextColor((uint32_t)0x666666);
            _canvas->setFont(&fonts::Font0);
            _canvas->drawString("Scroll: Knob  |  Click: Open", 120, 224);

            _canvas->pushSprite(0, 0);
        }
};