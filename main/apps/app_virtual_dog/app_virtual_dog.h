/**
 * @file app_virtual_dog.h
 * @brief Charming Retro Pixel Art Virtual Dog Pet App for M5Dial
 * @version 1.0
 * @date 2026-11-20
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include <string>
#include <vector>

namespace MOONCAKE
{
    namespace USER_APP
    {
        namespace VIRTUAL_DOG
        {
            enum AppMode_t
            {
                MODE_NAMING = 0,
                MODE_PET,
                MODE_STATS
            };

            enum PetAction_t
            {
                ACTION_FEED = 0,
                ACTION_PLAY,
                ACTION_PET,
                ACTION_SLEEP,
                ACTION_BATH,
                ACTION_STATS,
                ACTION_COUNT
            };

            enum AnimState_t
            {
                ANIM_IDLE = 0,
                ANIM_HAPPY,
                ANIM_EATING,
                ANIM_PLAYING,
                ANIM_SLEEPING,
                ANIM_BATH
            };

            struct Particle_t
            {
                float x;
                float y;
                float vx;
                float vy;
                uint8_t type; // 0: Heart, 1: Bubble, 2: Zzz, 3: Star
                uint8_t life;
                uint8_t max_life;
            };

            struct Ball_t
            {
                float x;
                float y;
                float vx;
                float vy;
                bool active;
            };

            struct Bone_t
            {
                float x;
                float y;
                float vy;
                bool active;
                bool caught;
                uint32_t catch_time;
            };

            struct Stats_t
            {
                std::string name;
                int hunger;       // 0 - 100
                int happiness;    // 0 - 100
                int energy;       // 0 - 100
                int cleanliness;  // 0 - 100
                int level;        // 1+
                int exp;          // 0 - 100
                int stage;        // 0: Puppy, 1: Adult Shiba
            };

            struct Data_t
            {
                HAL::HAL* hal = nullptr;
                AppMode_t mode = MODE_PET;
                AnimState_t anim_state = ANIM_IDLE;
                uint32_t state_start_time = 0;
                uint32_t last_tick_time = 0;
                uint32_t last_idle_anim_time = 0;
                uint32_t last_save_time = 0;
                int idle_frame = 0;

                // Pet Stats
                Stats_t stats;

                // Actions Carousel
                int selected_action = 0;

                // Naming Mode State (Dial-Picker)
                std::string input_name;
                int name_char_idx = 0;
                uint32_t cursor_blink_time = 0;
                bool cursor_visible = true;

                // Particles & FX
                std::vector<Particle_t> particles;
                Ball_t ball;
                Bone_t bone;
                std::string toast_msg;
                uint32_t toast_start_time = 0;
                uint32_t toast_color = 0xFFFF;
            };
        }

        class VirtualDogApp : public APP_BASE
        {
            private:
                const char* _tag = "Virtual Dog";
                VIRTUAL_DOG::Data_t _data;

                // Storage
                void _load_stats();
                void _save_stats();

                // Naming Mode (Dial-Picker)
                void _naming_init();
                void _naming_update();
                void _naming_render();
                void _naming_handle_encoder();
                void _naming_handle_touch();
                void _naming_commit_char();

                // Pet Mode
                void _pet_update();
                void _pet_render();
                void _pet_handle_encoder();
                void _pet_handle_touch();
                void _trigger_action(VIRTUAL_DOG::PetAction_t action);
                void _gain_exp(int amount);

                // Stats Mode
                void _stats_update();
                void _stats_render();
                void _stats_handle_encoder();
                void _stats_handle_touch();

                // Rendering Helpers
                void _draw_pixel_sprite(int cx, int cy, const uint16_t* sprite, int scale, int bob_y);
                void _draw_pixel_bone(int x, int y, int bob);
                void _spawn_particle(uint8_t type, float x, float y);
                void _update_particles();
                void _draw_particles();
                void _show_toast(const std::string& msg, uint32_t color = 0xFFFF);
                void _draw_toast();

                // Sound FX
                void _sound_bark();
                void _sound_happy_bark();
                void _sound_eat();
                void _sound_bounce();
                void _sound_purr();
                void _sound_fanfare();
                void _sound_click();
                void _sound_pop();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };

        void run_virtual_dog_app(HAL::HAL* hal);
    }
}
