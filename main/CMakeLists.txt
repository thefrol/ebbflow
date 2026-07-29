idf_component_register(
    SRCS "main.c" "settings.c" "wifi_setup.c" "time_sync.c" "lamp.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_driver_gpio
)
