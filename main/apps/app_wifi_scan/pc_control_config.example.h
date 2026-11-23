#pragma once

// Wi-Fi Access Point credentials for the local network shared by PC and M5Dial
#define PC_WIFI_SSID       "YOUR_WIFI_SSID"
#define PC_WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// PC companion server URL (replace with your PC's LAN IPv4 address and port)
#define PC_CONTROL_BASE_URL "http://192.168.0.25:8765"

// Secret authentication token — must match PC_CONTROLLER_TOKEN on the Windows PC
#define PC_CONTROLLER_TOKEN "YOUR_SECURE_CONTROLLER_TOKEN"
