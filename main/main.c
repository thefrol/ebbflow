#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "lamp.h"
#include "settings.h"
#include "time_sync.h"
#include "wifi_setup.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "ebbflow-lamp '%s': старт", CONFIG_LAMP_DEVICE_NAME);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    lamp_settings_t settings;
    if (settings_load(&settings) != ESP_OK) {
        ESP_LOGW(TAG, "настройки не прочитаны, используем дефолты menuconfig");
    }
    ESP_ERROR_CHECK(lamp_init(&settings));

    wifi_setup_connect(); // блокируется до получения IP, сама переподключается
    time_sync_start();    // блокируется до синхронизации времени

    lamp_start();
    ESP_LOGI(TAG, "инициализация завершена");
}
