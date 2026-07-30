idf_component_register(
    SRCS "main.c" "settings.c" "wifi_setup.c" "time_sync.c" "lamp.c" "ota_update.c"
         "web_server.c" "schedule.c" "hhmm.c"
    INCLUDE_DIRS "."
    EMBED_FILES "web/index.html"
    REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_driver_gpio
             esp_https_ota esp_http_client esp_http_server esp_app_format espressif__cjson
    PRIV_REQUIRES mbedtls app_update
)
