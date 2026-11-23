/**
 * @file app_vault.h
 * @brief Secret Msg Vault App for M5Dial
 * @version 1.0
 * @date 2026-11-20
 */
#pragma once
#include "../app.h"
#include "../../hal/hal.h"
#include "app_vault_heart.h"
#include <string>
#include <vector>

namespace MOONCAKE
{
    namespace USER_APP
    {
        enum VaultState_t
        {
            VAULT_INPUT = 0,
            VAULT_BREATHING,
            VAULT_BURST,
            VAULT_REVEAL
        };

        struct VaultData_t
        {
            HAL::HAL* hal = nullptr;
            VaultState_t state = VAULT_INPUT;
            uint32_t state_start_time = 0;

            // Message Input
            std::string message;
            int char_idx = 0;
            uint32_t cursor_blink_time = 0;
            bool cursor_visible = true;

            // Heart Breathing & Burst
            float heart_scale = 4.5f;
            uint32_t last_beat_time = 0;
            bool beat_phase = false;
            uint32_t burst_start_time = 0;
            float burst_scale = 5.0f;

            // Particles & FX
            std::vector<VAULT::Particle_t> particles;
            uint32_t last_ambient_spawn = 0;
        };

        class VaultApp : public APP_BASE
        {
            private:
                const char* _tag = "Vault";
                VaultData_t _data;

                // State Handlers
                void _input_update();
                void _input_render();
                void _input_handle_encoder();
                void _input_handle_touch();
                void _commit_char();

                void _breathing_update();
                void _breathing_render();
                void _breathing_handle_input();

                void _burst_update();
                void _burst_render();

                void _reveal_update();
                void _reveal_render();
                void _reveal_handle_input();

                // Graphics & FX Helpers
                void _draw_pixel_heart(int cx, int cy, float scale);
                void _spawn_burst_particles();
                void _spawn_ambient_heart();
                void _update_particles();
                void _draw_particles();
                void _draw_wrapped_text(const std::string& text, int cx, int cy, int max_w, uint32_t color);

                // Sound Effects
                void _sound_click();
                void _sound_heartbeat();
                void _sound_burst();
                void _sound_reveal();
                void _sound_chime();
                void _sound_fanfare();

            public:
                GUI_Base* getGui() override { return nullptr; }

                void onSetup() override;
                void onCreate() override;
                void onRunning() override;
                void onDestroy() override;
        };
    }
}
