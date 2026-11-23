
#include "wifi_common_test.h"

#include <string.h>
#include <assert.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

namespace WIFI_COMMON_TEST
{
    namespace SCAN
    {
        #define DEFAULT_SCAN_LIST_SIZE 10

        static const char *TAG = "scan";

        static void print_auth_mode(int authmode)
        {
            switch (authmode)
            {
                case WIFI_AUTH_OPEN:
                    ESP_LOGI(TAG, "Auth: OPEN");
                    break;
                case WIFI_AUTH_OWE:
                    ESP_LOGI(TAG, "Auth: OWE");
                    break;
                case WIFI_AUTH_WEP:
                    ESP_LOGI(TAG, "Auth: WEP");
                    break;
                case WIFI_AUTH_WPA_PSK:
                    ESP_LOGI(TAG, "Auth: WPA");
                    break;
                case WIFI_AUTH_WPA2_PSK:
                    ESP_LOGI(TAG, "Auth: WPA2");
                    break;
                case WIFI_AUTH_WPA_WPA2_PSK:
                    ESP_LOGI(TAG, "Auth: WPA/WPA2");
                    break;
                case WIFI_AUTH_WPA2_ENTERPRISE:
                    ESP_LOGI(TAG, "Auth: WPA2 Enterprise");
                    break;
                case WIFI_AUTH_WPA3_PSK:
                    ESP_LOGI(TAG, "Auth: WPA3");
                    break;
                case WIFI_AUTH_WPA2_WPA3_PSK:
                    ESP_LOGI(TAG, "Auth: WPA2/WPA3");
                    break;
                default:
                    ESP_LOGI(TAG, "Auth: Unknown");
                    break;
            }
        }

        /*
         * Runs one scan and stores results in a temporary array.
         * Returns the number of access points found.
         */
        static uint8_t perform_scan(
            wifi_ap_record_t *ap_info,
            uint16_t *ap_count)
        {
            esp_err_t err;
            bool nvs_ready = false;
            bool netif_ready = false;
            bool event_loop_ready = false;
            bool sta_netif_created = false;
            bool wifi_initialized = false;
            bool wifi_started = false;

            esp_netif_t *sta_netif = nullptr;

            *ap_count = 0;
            memset(ap_info, 0,
                   sizeof(wifi_ap_record_t) * DEFAULT_SCAN_LIST_SIZE);

            /*
             * Initialize NVS.
             * Do not erase NVS unless the documented NVS recovery
             * error is returned.
             */
            err = nvs_flash_init();

            if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
                err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                err = nvs_flash_erase();

                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "NVS erase failed: %s",
                             esp_err_to_name(err));
                    return 0;
                }

                err = nvs_flash_init();
            }

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "NVS init failed: %s",
                         esp_err_to_name(err));
                return 0;
            }

            nvs_ready = true;

            /*
             * Initialize network interfaces and event loop.
             * If either is already initialized, report the condition
             * rather than triggering ESP_ERROR_CHECK and rebooting.
             */
            err = esp_netif_init();

            if (err == ESP_OK)
            {
                netif_ready = true;
            }
            else if (err != ESP_ERR_INVALID_STATE)
            {
                ESP_LOGE(TAG, "Network interface init failed: %s",
                         esp_err_to_name(err));
                goto cleanup;
            }

            err = esp_event_loop_create_default();

            if (err == ESP_OK)
            {
                event_loop_ready = true;
            }
            else if (err != ESP_ERR_INVALID_STATE)
            {
                ESP_LOGE(TAG, "Event loop init failed: %s",
                         esp_err_to_name(err));
                goto cleanup;
            }

            /*
             * Create the default station interface.
             */
            sta_netif = esp_netif_create_default_wifi_sta();

            if (sta_netif == nullptr)
            {
                ESP_LOGE(TAG,
                         "Could not create Wi-Fi station interface. "
                         "Check whether another app already created it.");
                goto cleanup;
            }

            sta_netif_created = true;

            /*
             * Initialize Wi-Fi.
             */
            {
                wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

                err = esp_wifi_init(&cfg);

                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "Wi-Fi init failed: %s",
                             esp_err_to_name(err));
                    goto cleanup;
                }

                wifi_initialized = true;
            }

            err = esp_wifi_set_mode(WIFI_MODE_STA);

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Setting station mode failed: %s",
                         esp_err_to_name(err));
                goto cleanup;
            }

            err = esp_wifi_start();

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Starting Wi-Fi failed: %s",
                         esp_err_to_name(err));
                goto cleanup;
            }

            wifi_started = true;

            /*
             * Start a blocking scan and CHECK its return value.
             */
            err = esp_wifi_scan_start(nullptr, true);

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Wi-Fi scan start failed: %s",
                         esp_err_to_name(err));
                goto cleanup;
            }

            err = esp_wifi_scan_get_ap_num(ap_count);

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Getting AP count failed: %s",
                         esp_err_to_name(err));
                *ap_count = 0;
                goto cleanup;
            }

            {
                uint16_t records_to_read = DEFAULT_SCAN_LIST_SIZE;

                err = esp_wifi_scan_get_ap_records(
                    &records_to_read, ap_info);

                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "Getting AP records failed: %s",
                             esp_err_to_name(err));
                    *ap_count = 0;
                    goto cleanup;
                }

                if (*ap_count > records_to_read)
                {
                    *ap_count = records_to_read;
                }
            }

            ESP_LOGI(TAG, "Total access points found: %u",
                     (unsigned)*ap_count);

            for (uint16_t i = 0; i < *ap_count; ++i)
            {
                ESP_LOGI(TAG, "SSID: %s",
                         (const char *)ap_info[i].ssid);
                ESP_LOGI(TAG, "RSSI: %d", ap_info[i].rssi);
                ESP_LOGI(TAG, "Channel: %d", ap_info[i].primary);
                print_auth_mode(ap_info[i].authmode);
            }

        cleanup:

            if (wifi_started)
            {
                err = esp_wifi_stop();

                if (err != ESP_OK)
                {
                    ESP_LOGW(TAG, "Wi-Fi stop: %s",
                             esp_err_to_name(err));
                }
            }

            if (wifi_initialized)
            {
                err = esp_wifi_deinit();

                if (err != ESP_OK)
                {
                    ESP_LOGW(TAG, "Wi-Fi deinit: %s",
                             esp_err_to_name(err));
                }
            }

            if (sta_netif_created)
            {
                esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
                esp_netif_destroy_default_wifi(sta_netif);
            }

            if (event_loop_ready)
            {
                err = esp_event_loop_delete_default();

                if (err != ESP_OK)
                {
                    ESP_LOGW(TAG, "Event loop cleanup: %s",
                             esp_err_to_name(err));
                }
            }

            if (netif_ready)
            {
                err = esp_netif_deinit();

                if (err != ESP_OK)
                {
                    ESP_LOGW(TAG, "Network cleanup: %s",
                             esp_err_to_name(err));
                }
            }

            /*
             * Leave NVS initialized for the rest of the firmware.
             * Do not deinitialize shared NVS after every scan.
             */
            (void)nvs_ready;

            return (*ap_count > 0) ? (uint8_t)*ap_count : 0;
        }

        uint8_t scan(std::string& result)
        {
            wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
            uint16_t ap_count = 0;

            result.clear();

            ESP_LOGI(TAG, "Starting string-based Wi-Fi scan");

            uint8_t wifi_num = perform_scan(ap_info, &ap_count);

            for (uint16_t i = 0; i < ap_count; ++i)
            {
                result += std::to_string(i);
                result += ". (";
                result += std::to_string(ap_info[i].rssi);
                result += ") ";
                result += reinterpret_cast<const char *>(
                    ap_info[i].ssid);
                result += "\n";
            }

            if (ap_count == 0)
            {
                result = "No Wi-Fi networks found";
                ESP_LOGW(TAG, "Scan returned zero networks");
            }

            return wifi_num;
        }

        void scan(std::vector<ScanResult_t>& scanResult)
        {
            wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
            uint16_t ap_count = 0;

            scanResult.clear();

            ESP_LOGI(TAG, "Starting vector-based Wi-Fi scan");

            perform_scan(ap_info, &ap_count);

            for (uint16_t i = 0; i < ap_count; ++i)
            {
                ScanResult_t item;
                item.ssid = reinterpret_cast<const char *>(
                    ap_info[i].ssid);
                item.rssi = ap_info[i].rssi;

                scanResult.push_back(item);
            }

            if (ap_count == 0)
            {
                ESP_LOGW(TAG, "No Wi-Fi networks found");
            }
        }
    }
}
