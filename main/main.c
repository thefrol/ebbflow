#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "lamp.h"
#include "mdns_discovery.h"
#include "ota_update.h"
#include "settings.h"
#include "time_sync.h"
#include "web_server.h"
#include "wifi_setup.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "ebbflow-lamp '%s' v%s: старт",
             CONFIG_LAMP_DEVICE_NAME, esp_app_get_description()->version);

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

    wifi_setup_connect(&settings); // блокируется до получения IP, сама переподключается
    time_sync_start();    // блокируется до синхронизации времени
    mdns_discovery_init(&settings);
    web_server_start(&settings);

#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    // Самотест пройден (Wi-Fi + время есть) — подтверждаем OTA-образ,
    // иначе загрузчик откатится на предыдущий слот.
    esp_ota_mark_app_valid_cancel_rollback();
#endif

    lamp_start();

#if CONFIG_LAMP_OTA_ENABLED
    ota_update_start();
#endif

    ESP_LOGI(TAG, "инициализация завершена");
}
