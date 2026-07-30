#include "settings.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "hhmm.h"

static const char *TAG = "settings";

#define NVS_NAMESPACE "lamp"

static int parse_hhmm(const char *s, int fallback)
{
    int min = 0;
    if (hhmm_to_min(s, &min)) {
        return min;
    }
    ESP_LOGW(TAG, "не смог разобрать время '%s', беру дефолт %d:%02d",
             s, fallback / 60, fallback % 60);
    return fallback;
}

// Читает ключ из NVS; если его нет — записывает текущее значение как дефолт.
static esp_err_t get_or_seed_i32(nvs_handle_t handle, const char *key, int32_t *value)
{
    esp_err_t err = nvs_get_i32(handle, key, value);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "первый запуск: %s = %ld (из menuconfig)", key, (long)*value);
        err = nvs_set_i32(handle, key, *value);
    }
    return err;
}

esp_err_t settings_load(lamp_settings_t *out)
{
    // Дефолты из Kconfig
    out->gpio = CONFIG_LAMP_GPIO;
    out->on_min = parse_hhmm(CONFIG_LAMP_ON_TIME, 6 * 60);
    out->off_min = parse_hhmm(CONFIG_LAMP_OFF_TIME, 23 * 60);
    out->enabled = true;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }

    int32_t on_min = out->on_min;
    int32_t off_min = out->off_min;
    int32_t gpio = out->gpio;
    int32_t enabled = out->enabled;

    err = get_or_seed_i32(handle, "on_min", &on_min);
    if (err == ESP_OK) err = get_or_seed_i32(handle, "off_min", &off_min);
    if (err == ESP_OK) err = get_or_seed_i32(handle, "gpio", &gpio);
    if (err == ESP_OK) err = get_or_seed_i32(handle, "enabled", &enabled);
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ошибка NVS: %s, работаем на дефолтах из menuconfig",
                 esp_err_to_name(err));
        return err;
    }

    out->on_min = on_min;
    out->off_min = off_min;
    out->gpio = gpio;
    out->enabled = enabled != 0;
    return ESP_OK;
}

esp_err_t settings_save(const lamp_settings_t *settings)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_i32(handle, "on_min", settings->on_min);
    if (err == ESP_OK) err = nvs_set_i32(handle, "off_min", settings->off_min);
    if (err == ESP_OK) err = nvs_set_i32(handle, "gpio", settings->gpio);
    if (err == ESP_OK) err = nvs_set_i32(handle, "enabled", settings->enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);
    return err;
}
