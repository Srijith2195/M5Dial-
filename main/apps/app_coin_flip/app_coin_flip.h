/**
 * @file app_coin_flip.h
 * @brief 3D Animated Coin Flip Decision Maker for M5Dial
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
        namespace COIN_FLIP
        {
            enum Side_t { HEADS, TAILS };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                Side_t current_side = HEADS;
                bool is_flipping = false;
                int flip_frame = 0;
                int heads_count = 0;
                int tails_count = 0;
            };
        }

        class CoinFlipApp : public APP_BASE
        {
            private:
                const char* _tag = "coin";
                COIN_FLIP::Data_t _data;

                void _flip();
                void _render();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_coin_flip_app(HAL::HAL* hal);
    }
}
