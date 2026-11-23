/**
 * @file app_breakout.h
 * @brief Retro Rotary Breakout / Pong Arcade Game for M5Dial
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
        namespace BREAKOUT
        {
            struct Brick_t
            {
                int x, y, w, h;
                uint32_t color;
                int points;
                bool alive;
            };

            enum GameState_t { READY, PLAYING, LIFE_LOST, GAME_OVER, VICTORY };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                GameState_t state = READY;
                float paddle_x = 120.0f;
                int paddle_w = 44;
                int paddle_h = 7;
                int paddle_y = 202;

                float ball_x = 120.0f;
                float ball_y = 194.0f;
                float ball_vx = 2.2f;
                float ball_vy = -3.2f;
                int ball_r = 4;

                Brick_t bricks[30];
                int bricks_left = 30;
                int score = 0;
                int lives = 3;
                uint32_t last_frame_time = 0;
            };
        }

        class BreakoutApp : public APP_BASE
        {
            private:
                const char* _tag = "breakout";
                BREAKOUT::Data_t _data;

                void _init_game();
                void _reset_ball();
                void _update_physics();
                void _render();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_breakout_app(HAL::HAL* hal);
    }
}
