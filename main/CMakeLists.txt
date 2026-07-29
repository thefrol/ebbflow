idf_component_register(
    SRCS "main.c" "settings.c" "wifi_setup.c" "time_sync.c" "lamp.c" "ota_update.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_driver_gpio
             esp_https_ota esp_http_client esp_app_format espressif__cjson
    PRIV_REQUIRES mbedtls app_update
)
