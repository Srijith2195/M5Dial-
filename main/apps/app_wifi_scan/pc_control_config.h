#pragma once

// Primary: Direct 2.4 GHz Windows PC Hotspot (Bypasses office 5 GHz, client isolation, and firewalls)
#define PC_WIFI_SSID       "M5Dial_PC"
#define PC_WIFI_PASSWORD   "@Schne11"
#define PC_CONTROL_BASE_URL "http://192.168.137.1:8765"

// Secondary fallback: Office / Router Wi-Fi
#define PC_WIFI_SSID_ALT   "HAS_IT"
#define PC_WIFI_PASS_ALT   "@Schne11"
#define PC_CONTROL_URL_ALT "http://192.168.0.25:8765"

// Authentication token — must match the PC server
#define PC_CONTROLLER_TOKEN "M5Dial-PC-7f9C2mQ8vL4xN6pR5tY3kW1zH9aB"

