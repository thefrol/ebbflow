#include "ota_update.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "ota";

// Буфер для тела ответа GitHub API. JSON релиза с парой ассетов — десятки
// килобайт из-за метаданных, берём с запасом; переполнение обрывает запрос.
#define API_BUF_CAP (32 * 1024)

typedef struct {
    char *buf;
    size_t len;
} http_body_t;

static esp_err_t http_on_event(esp_http_client_event_t *evt)
{
    http_body_t *body = evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (body->len + evt->data_len >= API_BUF_CAP) {
            ESP_LOGE(TAG, "ответ API не влез в %d байт", API_BUF_CAP);
            return ESP_FAIL;
        }
        memcpy(body->buf + body->len, evt->data, evt->data_len);
        body->len += evt->data_len;
    }
    return ESP_OK;
}

// Скачивает URL целиком в память, возвращает буфер (освобождает вызывающий)
// или NULL при ошибке.
static char *http_get(const char *url)
{
    http_body_t body = {.buf = malloc(API_BUF_CAP), .len = 0};
    if (!body.buf) {
        return NULL;
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = http_on_event,
        .user_data = &body,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    // GitHub API отвечает 403 на запросы без User-Agent
    esp_http_client_set_header(client, "User-Agent", "ebbflow-lamp-ota");
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "GET %s: err=%s, HTTP %d", url, esp_err_to_name(err), status);
        free(body.buf);
        return NULL;
    }
    body.buf[body.len] = '\0';
    return body.buf;
}

// "1.2.3" или "v1.2.3" -> {1,2,3}. false, если не похоже на semver.
static bool parse_version(const char *s, int v[3])
{
    if (s[0] == 'v') {
        s++;
    }
    return sscanf(s, "%d.%d.%d", &v[0], &v[1], &v[2]) == 3;
}

// Тег релиза новее текущей прошивки?
static bool release_is_newer(const char *tag)
{
    const char *t = (tag[0] == 'v') ? tag + 1 : tag;
    const char *cur = esp_app_get_description()->version;
    if (strcmp(cur, t) == 0) {
        return false;
    }
    int c[3], r[3];
    if (!parse_version(cur, c)) {
        // Своя версия не semver (локальная сборка) — считаем релиз новее
        return true;
    }
    if (!parse_version(t, r)) {
        ESP_LOGW(TAG, "тег релиза '%s' не похож на версию, игнорирую", tag);
        return false;
    }
    for (int i = 0; i < 3; i++) {
        if (r[i] != c[i]) {
            return r[i] > c[i];
        }
    }
    return false;
}

// Ищет в JSON релиза ассет под текущий чип, возвращает его download URL
// (строка внутри json — живёт, пока жив json) или NULL.
static const char *find_asset_url(cJSON *json, char *out_tag, size_t tag_size)
{
    cJSON *tag = cJSON_GetObjectItemCaseSensitive(json, "tag_name");
    if (!cJSON_IsString(tag)) {
        return NULL;
    }
    snprintf(out_tag, tag_size, "%s", tag->valuestring);

    char asset_name[64];
    snprintf(asset_name, sizeof(asset_name), "ebbflow-lamp-%s.bin", CONFIG_IDF_TARGET);

    cJSON *assets = cJSON_GetObjectItemCaseSensitive(json, "assets");
    cJSON *asset;
    cJSON_ArrayForEach(asset, assets) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(asset, "name");
        if (cJSON_IsString(name) && strcmp(name->valuestring, asset_name) == 0) {
            cJSON *url = cJSON_GetObjectItemCaseSensitive(asset, "browser_download_url");
            return cJSON_IsString(url) ? url->valuestring : NULL;
        }
    }
    ESP_LOGW(TAG, "в релизе нет ассета %s", asset_name);
    return NULL;
}

static void ota_from_url(const char *url)
{
    ESP_LOGI(TAG, "скачиваю обновление: %s", url);
    esp_http_client_config_t http_cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
        // После 302-редиректа GitHub отдаёт подписанный URL длиной ~900+
        // байт — в дефолтный буфер 512 он не влезает и запрос обрывается
        // (та же ошибка ломала OTA у ESPHome, esphome#13786).
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };
    esp_err_t err = esp_https_ota(&ota_cfg);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "обновление записано, перезагрузка");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    ESP_LOGE(TAG, "OTA не удалось: %s", esp_err_to_name(err));
}

static void ota_check_once(void)
{
    char api_url[128];
    snprintf(api_url, sizeof(api_url),
             "https://api.github.com/repos/%s/releases/latest", CONFIG_LAMP_OTA_REPO);

    char *body = http_get(api_url);
    if (!body) {
        return;
    }
    cJSON *json = cJSON_Parse(body);
    free(body);
    if (!json) {
        ESP_LOGW(TAG, "не смог разобрать JSON ответа API");
        return;
    }

    char tag[32] = {0};
    const char *bin_url = find_asset_url(json, tag, sizeof(tag));
    const char *cur = esp_app_get_description()->version;

    if (bin_url && release_is_newer(tag)) {
        ESP_LOGI(TAG, "есть новая версия: %s (у меня %s)", tag, cur);
        ota_from_url(bin_url); // при успехе не вернётся — устройство перезагрузится
    } else if (bin_url) {
        ESP_LOGI(TAG, "версия актуальна: %s (последний релиз %s)", cur, tag);
    }
    cJSON_Delete(json);
}

static void ota_task(void *arg)
{
    for (;;) {
        ota_check_once();
        vTaskDelay(pdMS_TO_TICKS(CONFIG_LAMP_OTA_CHECK_INTERVAL_MIN * 60 * 1000));
    }
}

void ota_update_start(void)
{
    ESP_LOGI(TAG, "проверяю обновления в %s каждые %d мин",
             CONFIG_LAMP_OTA_REPO, CONFIG_LAMP_OTA_CHECK_INTERVAL_MIN);
    xTaskCreate(ota_task, "ota", 8192, NULL, 4, NULL);
}
