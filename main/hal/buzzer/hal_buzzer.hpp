/**
 * @file hal_buzzer.hpp
 * @author Forairaaaaa
 * @brief Hardware LEDC Buzzer with NVS Sound Enable/Disable Toggle
 * @version 0.2
 * @date 2026-11-17
 */
#pragma once
#include <stdint.h>
#include "driver/ledc.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"

namespace BUZZER {

    class BUZZER {
        private:
            int _pin = 3;
            esp_timer_handle_t _timer = nullptr;
            bool _inited = false;
            bool _enabled = true;

            static void _timer_cb(void* arg) {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
            }

            void _load_pref() {
                nvs_handle_t h;
                if (nvs_open("m5_cfg", NVS_READONLY, &h) == ESP_OK) {
                    uint8_t val = 1;
                    if (nvs_get_u8(h, "sound_en", &val) == ESP_OK) {
                        _enabled = (val != 0);
                    }
                    nvs_close(h);
                }
            }

            void _save_pref() {
                nvs_handle_t h;
                if (nvs_open("m5_cfg", NVS_READWRITE, &h) == ESP_OK) {
                    nvs_set_u8(h, "sound_en", _enabled ? 1 : 0);
                    nvs_commit(h);
                    nvs_close(h);
                }
            }

        public:
            BUZZER() : _pin(3) {}

            void init(int8_t pin) {
                _pin = pin;

                // Load saved sound preference (default: enabled)
                _load_pref();

                ledc_timer_config_t t = {};
                t.speed_mode = LEDC_LOW_SPEED_MODE;
                t.duty_resolution = LEDC_TIMER_10_BIT;
                t.timer_num = LEDC_TIMER_1;
                t.freq_hz = 2000;
                t.clk_cfg = LEDC_AUTO_CLK;
                ledc_timer_config(&t);

                ledc_channel_config_t c = {};
                c.gpio_num = _pin;
                c.speed_mode = LEDC_LOW_SPEED_MODE;
                c.channel = LEDC_CHANNEL_1;
                c.timer_sel = LEDC_TIMER_1;
                c.duty = 0;
                c.hpoint = 0;
                ledc_channel_config(&c);

                if (_timer == nullptr) {
                    esp_timer_create_args_t args = {};
                    args.callback = _timer_cb;
                    args.name = "buzzer";
                    esp_timer_create(&args, &_timer);
                }
                _inited = true;
            }

            bool isEnabled() const {
                return _enabled;
            }

            void setEnabled(bool en) {
                _enabled = en;
                _save_pref();
                if (!_enabled) {
                    noTone();
                }
            }

            void toggle() {
                setEnabled(!_enabled);
            }

            void tone(unsigned int frequency, unsigned long duration = 0) {
                if (!_enabled) return;
                if (!_inited) init(_pin);
                if (frequency < 100) {
                    noTone();
                    return;
                }
                if (_timer != nullptr) {
                    esp_timer_stop(_timer);
                }
                ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, frequency);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 512); // 50% duty square wave
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

                if (duration > 0 && _timer != nullptr) {
                    esp_timer_start_once(_timer, (uint64_t)duration * 1000ULL);
                }
            }

            void noTone() {
                if (_timer != nullptr) {
                    esp_timer_stop(_timer);
                }
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
            }
    };

}
