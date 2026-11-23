# HAL
file(GLOB_RECURSE HAL_SRCS
    hal/*.c
    hal/*.cpp
)

set(HAL_INCS
    hal/
)

# Applications
file(GLOB_RECURSE APP_SRCS
    apps/*.c
    apps/*.cpp
)

set(APP_INCS
    apps/
)

idf_component_register(
    SRCS
        "main.cpp"
        ${HAL_SRCS}
        ${APP_SRCS}

    INCLUDE_DIRS
        "."
        ${HAL_INCS}
        ${APP_INCS}

    REQUIRES
    LovyanGFX
    lvgl
    fatfs
    esp_adc
    esp-idf-rc522
    bt
    esp_http_client
    esp_wifi
    esp_event
    esp_netif
    nvs_flash
    json
)