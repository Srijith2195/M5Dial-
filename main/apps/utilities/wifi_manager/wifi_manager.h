#pragma once

#include <string>
#include <vector>
#include <stdint.h>

namespace WIFI_MANAGER {
    struct Network {
        std::string ssid;
        int8_t rssi = 0;
        bool secure = false;
    };

    enum class State {
        NOT_INITIALIZED,
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        ERROR
    };

    bool init();
    bool scan(std::vector<Network>& networks);
    bool connect(const std::string& ssid, const std::string& password);
    State state();
    std::string connectedSSID();
    std::string ipv4();
    std::string lastError();
    bool hasSavedCredentials();
}
