
#include "wifi_manager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "lwip/inet.h"

namespace {

constexpr const char* TAG = "wifi_manager";
constexpr const char* NVS_NAMESPACE = "wifi_mgr";
constexpr const char* NVS_SSID = "ssid";
constexpr const char* NVS_PASSWORD = "password";

bool s_initialized = false;
esp_netif_t* s_sta_netif = nullptr;

esp_event_handler_instance_t s_wifi_handler = nullptr;
esp_event_handler_instance_t s_ip_handler = nullptr;

WIFI_MANAGER::State s_state =
    WIFI_MANAGER::State::NOT_INITIALIZED;

char s_connected_ssid[33] = {};
char s_ip[16] = {};
char s_error[96] = {};

SemaphoreHandle_t s_mutex = nullptr;
unsigned s_disconnect_attempts = 0;

void lockState() {
    if (s_mutex) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
}

void unlockState() {
    if (s_mutex) {
        xSemaphoreGive(s_mutex);
    }
}

void setState(WIFI_MANAGER::State state) {
    lockState();
    s_state = state;
    unlockState();
}

void setError(const char* message) {
    lockState();

    snprintf(
        s_error,
        sizeof(s_error),
        "%s",
        message ? message : "Unknown error"
    );

    s_state = WIFI_MANAGER::State::ERROR;

    unlockState();

    ESP_LOGE(TAG, "%s", message ? message : "Unknown error");
}

bool ensureNVS() {
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();

        if (err == ESP_OK) {
            err = nvs_flash_init();
        }
    }

    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        return true;
    }

    ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
    return false;
}

void eventHandler(
    void*,
    esp_event_base_t base,
    int32_t id,
    void* data
) {
    if (base == WIFI_EVENT &&
        id == WIFI_EVENT_STA_DISCONNECTED) {

        lockState();

        s_ip[0] = '\0';

        const bool should_retry =
            s_state == WIFI_MANAGER::State::CONNECTING ||
            s_state == WIFI_MANAGER::State::CONNECTED;

        const bool retry_allowed =
            should_retry && s_disconnect_attempts < 8;

        if (retry_allowed) {
            s_state = WIFI_MANAGER::State::CONNECTING;
        } else if (should_retry) {
            s_state = WIFI_MANAGER::State::ERROR;

            snprintf(
                s_error,
                sizeof(s_error),
                "%s",
                "Connection failed; check password/signal"
            );
        } else {
            s_state = WIFI_MANAGER::State::DISCONNECTED;
        }

        if (retry_allowed) {
            ++s_disconnect_attempts;
        }

        unlockState();

        if (retry_allowed) {
            ESP_LOGW(
                TAG,
                "Wi-Fi disconnected; retry %u/8",
                s_disconnect_attempts
            );

            esp_err_t err = esp_wifi_connect();

            if (err != ESP_OK) {
                ESP_LOGW(
                    TAG,
                    "Reconnect request: %s",
                    esp_err_to_name(err)
                );
            }
        } else if (should_retry) {
            ESP_LOGE(TAG, "Connection retry limit reached");
        }

    } else if (
        base == IP_EVENT &&
        id == IP_EVENT_STA_GOT_IP
    ) {
        auto* event = static_cast<ip_event_got_ip_t*>(data);

        char ip[16] = {};

        snprintf(
            ip,
            sizeof(ip),
            IPSTR,
            IP2STR(&event->ip_info.ip)
        );

        lockState();

        snprintf(s_ip, sizeof(s_ip), "%s", ip);

        s_state = WIFI_MANAGER::State::CONNECTED;
        s_error[0] = '\0';
        s_disconnect_attempts = 0;

        unlockState();

        ESP_LOGI(TAG, "Connected; IPv4 address: %s", ip);
    }
}

bool readCredentials(
    std::string& ssid,
    std::string& password
) {
    nvs_handle_t handle;

    if (nvs_open(
            NVS_NAMESPACE,
            NVS_READONLY,
            &handle
        ) != ESP_OK) {
        return false;
    }

    size_t ssid_len = 0;
    size_t pass_len = 0;

    esp_err_t a = nvs_get_str(
        handle, NVS_SSID, nullptr, &ssid_len
    );

    esp_err_t b = nvs_get_str(
        handle, NVS_PASSWORD, nullptr, &pass_len
    );

    if (a != ESP_OK ||
        b != ESP_OK ||
        ssid_len < 2 ||
        ssid_len > 33 ||
        pass_len > 65) {
        nvs_close(handle);
        return false;
    }

    std::vector<char> ssid_buf(ssid_len);
    std::vector<char> pass_buf(pass_len ? pass_len : 1);

    a = nvs_get_str(
        handle, NVS_SSID, ssid_buf.data(), &ssid_len
    );

    b = nvs_get_str(
        handle, NVS_PASSWORD, pass_buf.data(), &pass_len
    );

    nvs_close(handle);

    if (a != ESP_OK || b != ESP_OK) {
        return false;
    }

    ssid.assign(ssid_buf.data());
    password.assign(pass_buf.data());

    return !ssid.empty();
}

bool saveCredentials(
    const std::string& ssid,
    const std::string& password
) {
    nvs_handle_t handle;

    esp_err_t err = nvs_open(
        NVS_NAMESPACE,
        NVS_READWRITE,
        &handle
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not open saved credentials");
        return false;
    }

    err = nvs_set_str(handle, NVS_SSID, ssid.c_str());

    if (err == ESP_OK) {
        err = nvs_set_str(
            handle, NVS_PASSWORD, password.c_str()
        );
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not save Wi-Fi credentials: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    return true;
}

bool applyCredentials(
    const std::string& ssid,
    const std::string& password,
    bool save
) {
    if (ssid.empty() ||
        ssid.size() > 32 ||
        password.size() > 64 ||
        (!password.empty() && password.size() < 8)) {
        setError("SSID/password length is invalid");
        return false;
    }

    /*
     * Avoid resetting the Wi-Fi connection when this exact SSID
     * is already connected and has a valid station connection.
     */
    if (WIFI_MANAGER::state() == WIFI_MANAGER::State::CONNECTED &&
        WIFI_MANAGER::connectedSSID() == ssid) {

        wifi_ap_record_t current_ap = {};

        if (esp_wifi_sta_get_ap_info(&current_ap) == ESP_OK) {
            if (save && !saveCredentials(ssid, password)) {
                setError("Could not save Wi-Fi credentials");
                return false;
            }

            ESP_LOGI(
                TAG,
                "Already connected to %s; keeping current connection",
                ssid.c_str()
            );

            return true;
        }
    }

    if (save && !saveCredentials(ssid, password)) {
        setError("Could not save Wi-Fi credentials");
        return false;
    }

    wifi_config_t config = {};

    memcpy(config.sta.ssid, ssid.data(), ssid.size());
    memcpy(config.sta.password, password.data(), password.size());

    config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;
    config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    /*
     * If switching networks, stop the old connection first.
     * Mark the state DISCONNECTED so the disconnect event handler
     * does not start retrying the old connection.
     */
    wifi_ap_record_t current_ap = {};

    if (esp_wifi_sta_get_ap_info(&current_ap) == ESP_OK) {
        lockState();
        s_state = WIFI_MANAGER::State::DISCONNECTED;
        s_ip[0] = '\0';
        unlockState();

        esp_err_t disconnect_err = esp_wifi_disconnect();

        if (disconnect_err != ESP_OK &&
            disconnect_err != ESP_ERR_WIFI_NOT_CONNECT) {
            ESP_LOGW(
                TAG,
                "Disconnect before switching: %s",
                esp_err_to_name(disconnect_err)
            );
        }

        vTaskDelay(pdMS_TO_TICKS(150));
    }

    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &config);

    if (err != ESP_OK) {
        setError(esp_err_to_name(err));
        return false;
    }

    lockState();

    snprintf(
        s_connected_ssid,
        sizeof(s_connected_ssid),
        "%s",
        ssid.c_str()
    );

    s_ip[0] = '\0';
    s_error[0] = '\0';
    s_state = WIFI_MANAGER::State::CONNECTING;
    s_disconnect_attempts = 0;

    unlockState();

    err = esp_wifi_connect();

    if (err != ESP_OK) {
        setError(esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid.c_str());

    return true;
}

} // namespace

namespace WIFI_MANAGER {

bool init() {
    if (s_initialized) {
        return true;
    }

    if (!s_mutex) {
        s_mutex = xSemaphoreCreateMutex();

        if (!s_mutex) {
            ESP_LOGE(TAG, "Failed to create state mutex");
            return false;
        }
    }

    if (!ensureNVS()) {
        setError("NVS initialization failed");
        return false;
    }

    esp_err_t err = esp_netif_init();

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        setError(esp_err_to_name(err));
        return false;
    }

    err = esp_event_loop_create_default();

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        setError(esp_err_to_name(err));
        return false;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();

    if (!s_sta_netif) {
        setError("Could not create Wi-Fi station interface");
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    err = esp_wifi_init(&cfg);

    if (err != ESP_OK) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = nullptr;
        setError(esp_err_to_name(err));
        return false;
    }

    err = esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        eventHandler,
        nullptr,
        &s_wifi_handler
    );

    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            eventHandler,
            nullptr,
            &s_ip_handler
        );
    }

    if (err != ESP_OK) {
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = nullptr;
        setError(esp_err_to_name(err));
        return false;
    }

    err = esp_wifi_set_mode(WIFI_MODE_STA);

    if (err == ESP_OK) {
        err = esp_wifi_start();
    }

    if (err != ESP_OK) {
        setError(esp_err_to_name(err));
        return false;
    }

    s_initialized = true;
    setState(State::DISCONNECTED);

    std::string ssid;
    std::string password;

    if (readCredentials(ssid, password)) {
        ESP_LOGI(TAG, "Saved credentials found; reconnecting");
        applyCredentials(ssid, password, false);
    }

    return true;
}

bool scan(std::vector<Network>& networks) {
    networks.clear();

    if (!init()) {
        ESP_LOGE(TAG, "Wi-Fi manager initialization failed");
        return false;
    }

    /*
     * Wait for a connection attempt to finish before scanning.
     */
    for (int i = 0; i < 30; ++i) {
        if (state() != State::CONNECTING) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    if (state() == State::CONNECTING) {
        ESP_LOGW(
            TAG,
            "Connection still in progress; attempting scan"
        );
    }

    wifi_scan_config_t cfg = {};
    cfg.show_hidden = false;

    ESP_LOGI(TAG, "Starting Wi-Fi scan");

    esp_err_t err = esp_wifi_scan_start(&cfg, true);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Scan start failed: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    uint16_t count = 0;

    err = esp_wifi_scan_get_ap_num(&count);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not get AP count: %s",
            esp_err_to_name(err)
        );
        return false;
    }

    count = std::min<uint16_t>(count, 30);

    std::vector<wifi_ap_record_t> records(count);

    if (count > 0) {
        err = esp_wifi_scan_get_ap_records(
            &count,
            records.data()
        );

        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Could not retrieve AP records: %s",
                esp_err_to_name(err)
            );
            return false;
        }
    }

    for (uint16_t i = 0; i < count; ++i) {
        Network network;

        network.ssid = reinterpret_cast<const char*>(
            records[i].ssid
        );

        network.rssi = records[i].rssi;
        network.secure = records[i].authmode != WIFI_AUTH_OPEN;

        if (!network.ssid.empty()) {
            networks.push_back(network);
        }
    }

    std::sort(
        networks.begin(),
        networks.end(),
        [](const Network& a, const Network& b) {
            return a.rssi > b.rssi;
        }
    );

    ESP_LOGI(
        TAG,
        "Returning %u visible networks",
        static_cast<unsigned>(networks.size())
    );

    return true;
}

bool connect(
    const std::string& ssid,
    const std::string& password
) {
    if (!init()) {
        return false;
    }

    return applyCredentials(ssid, password, true);
}

State state() {
    lockState();
    State result = s_state;
    unlockState();
    return result;
}

std::string connectedSSID() {
    char value[sizeof(s_connected_ssid)];

    lockState();
    snprintf(value, sizeof(value), "%s", s_connected_ssid);
    unlockState();

    return value;
}

std::string ipv4() {
    char value[sizeof(s_ip)];

    lockState();
    snprintf(value, sizeof(value), "%s", s_ip);
    unlockState();

    return value;
}

std::string lastError() {
    char value[sizeof(s_error)];

    lockState();
    snprintf(value, sizeof(value), "%s", s_error);
    unlockState();

    return value;
}

bool hasSavedCredentials() {
    std::string ssid;
    std::string password;

    return readCredentials(ssid, password);
}

} // namespace WIFI_MANAGER
