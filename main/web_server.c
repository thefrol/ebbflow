#include "web_server.h"

#include <string.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "lamp.h"
#include "hhmm.h"

static const char *TAG = "web";

// Страничка встраивается в прошивку (EMBED_FILES "web/index.html").
extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

// Текущие настройки: копия от старта, обновляется из POST /api/settings.
// Пишется только из задачи httpd (один писатель), мьютекс не нужен.
static lamp_settings_t s_settings;

// Собирает JSON текущих настроек. Освобождать через cJSON_free.
static char *settings_to_json(const lamp_settings_t *s)
{
    char on[6], off[6];
    hhmm_min_to_str(s->on_min, on, sizeof(on));
    hhmm_min_to_str(s->off_min, off, sizeof(off));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "on", on);
    cJSON_AddStringToObject(root, "off", off);
    cJSON_AddNumberToObject(root, "gpio", s->gpio);
    cJSON_AddBoolToObject(root, "enabled", s->enabled);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static esp_err_t send_json(httpd_req_t *req, char *json)
{
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json, strlen(json));
    cJSON_free(json);
    return err;
}

static esp_err_t handle_get_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, index_html_start, index_html_end - index_html_start);
}

static esp_err_t handle_get_info(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", CONFIG_LAMP_DEVICE_NAME);
    cJSON_AddStringToObject(root, "version", esp_app_get_description()->version);
    cJSON_AddStringToObject(root, "chip", CONFIG_IDF_TARGET);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return send_json(req, json);
}

static esp_err_t handle_get_settings(httpd_req_t *req)
{
    return send_json(req, settings_to_json(&s_settings));
}

// Читает тело запроса целиком в buf (с '\0'). false — тело не влезло.
static bool read_body(httpd_req_t *req, char *buf, size_t len)
{
    if (req->content_len >= len) {
        return false;
    }
    size_t received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buf + received, req->content_len - received);
        if (ret <= 0) {
            return false;
        }
        received += ret;
    }
    buf[received] = '\0';
    return true;
}

static esp_err_t reject_bad_request(httpd_req_t *req, const char *reason)
{
    ESP_LOGW(TAG, "POST /api/settings отклонён: %s", reason);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, reason);
}

static esp_err_t handle_post_settings(httpd_req_t *req)
{
    char body[256];
    if (!read_body(req, body, sizeof(body))) {
        return reject_bad_request(req, "не смог прочитать тело запроса");
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        return reject_bad_request(req, "невалидный JSON");
    }

    lamp_settings_t next = s_settings;

    const cJSON *on = cJSON_GetObjectItem(root, "on");
    if (cJSON_IsString(on) && !hhmm_to_min(on->valuestring, &next.on_min)) {
        cJSON_Delete(root);
        return reject_bad_request(req, "on: ожидается HH:MM");
    }
    const cJSON *off = cJSON_GetObjectItem(root, "off");
    if (cJSON_IsString(off) && !hhmm_to_min(off->valuestring, &next.off_min)) {
        cJSON_Delete(root);
        return reject_bad_request(req, "off: ожидается HH:MM");
    }
    const cJSON *gpio = cJSON_GetObjectItem(root, "gpio");
    if (cJSON_IsNumber(gpio)) {
        // Диапазон как в Kconfig (LAMP_GPIO)
        if (gpio->valueint < 0 || gpio->valueint > 21) {
            cJSON_Delete(root);
            return reject_bad_request(req, "gpio: допустимо 0–21");
        }
        next.gpio = gpio->valueint;
    }
    const cJSON *enabled = cJSON_GetObjectItem(root, "enabled");
    if (cJSON_IsBool(enabled)) {
        next.enabled = cJSON_IsTrue(enabled);
    }
    cJSON_Delete(root);

    esp_err_t err = settings_save(&next);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "settings_save failed: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_sendstr(req, "не удалось сохранить в NVS");
    }

    s_settings = next;
    lamp_apply_settings(&next);
    ESP_LOGI(TAG, "настройки обновлены через веб: %02d:%02d–%02d:%02d, gpio %d, %s",
             next.on_min / 60, next.on_min % 60,
             next.off_min / 60, next.off_min % 60,
             next.gpio, next.enabled ? "включено" : "выключено");

    return send_json(req, settings_to_json(&s_settings));
}

void web_server_start(const lamp_settings_t *settings)
{
    s_settings = *settings;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "не удалось запустить HTTP-сервер");
        return;
    }

    const httpd_uri_t routes[] = {
        { .uri = "/", .method = HTTP_GET, .handler = handle_get_root },
        { .uri = "/api/info", .method = HTTP_GET, .handler = handle_get_info },
        { .uri = "/api/settings", .method = HTTP_GET, .handler = handle_get_settings },
        { .uri = "/api/settings", .method = HTTP_POST, .handler = handle_post_settings },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }

    ESP_LOGI(TAG, "веб-интерфейс запущен на порту %d", config.server_port);
}
