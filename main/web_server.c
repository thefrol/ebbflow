#include "web_server.h"

#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "lamp.h"
#include "hhmm.h"
#include "schedule.h"
#include "ota_update.h"
#include "mdns_discovery.h"

static const char *TAG = "web";

// Сжатый HTML frontend встраивается в прошивку (EMBED_FILES "web/dist/index.html.gz").
extern const char index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const char index_html_gz_end[] asm("_binary_index_html_gz_end");

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
    cJSON_AddStringToObject(root, "mode", s->mode == LAMP_MODE_PULSE ? "pulse" : "schedule");
    cJSON_AddStringToObject(root, "name", s->name);
    cJSON_AddStringToObject(root, "on", on);
    cJSON_AddStringToObject(root, "off", off);
    cJSON_AddNumberToObject(root, "pulse_interval_min", s->pulse_interval_min);
    cJSON_AddNumberToObject(root, "pulse_duration_sec", s->pulse_duration_sec);
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
    ESP_LOGW(TAG, "POST отклонён: %s", reason);
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_sendstr(req, reason);
}

static esp_err_t handle_get_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, index_html_gz_start, index_html_gz_end - index_html_gz_start);
}

static esp_err_t handle_get_info(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", s_settings.name);
    cJSON_AddStringToObject(root, "version", esp_app_get_description()->version);
    cJSON_AddStringToObject(root, "chip", CONFIG_IDF_TARGET);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return send_json(req, json);
}

#define PEERS_MAX 8

static esp_err_t handle_get_peers(httpd_req_t *req)
{
    mdns_peer_t peers[PEERS_MAX];
    int n = mdns_discovery_browse(peers, PEERS_MAX);

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", peers[i].id);
        cJSON_AddStringToObject(item, "name", peers[i].name);
        cJSON_AddStringToObject(item, "host", peers[i].host);
        cJSON_AddStringToObject(item, "ip", peers[i].ip);
        cJSON_AddNumberToObject(item, "port", peers[i].port);
        cJSON_AddStringToObject(item, "version", peers[i].version);
        cJSON_AddStringToObject(item, "chip", peers[i].chip);
        cJSON_AddStringToObject(item, "mode", peers[i].mode);
        cJSON_AddItemToArray(root, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return send_json(req, json);
}

static esp_err_t handle_get_settings(httpd_req_t *req)
{
    return send_json(req, settings_to_json(&s_settings));
}

static esp_err_t handle_get_status(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mode", s_settings.mode == LAMP_MODE_PULSE ? "pulse" : "schedule");
    cJSON_AddBoolToObject(root, "enabled", s_settings.enabled);
    cJSON_AddBoolToObject(root, "manual_pulse_active", lamp_manual_pulse_active());

    time_t next = lamp_next_pulse_time();
    if (next == (time_t)-1) {
        cJSON_AddNullToObject(root, "next_pulse_time");
        cJSON_AddBoolToObject(root, "next_pulse_today", false);
    } else {
        struct tm tm_next;
        localtime_r(&next, &tm_next);
        char buf[6];
        hhmm_min_to_str(tm_next.tm_hour * 60 + tm_next.tm_min, buf, sizeof(buf));
        cJSON_AddStringToObject(root, "next_pulse_time", buf);

        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        cJSON_AddBoolToObject(root, "next_pulse_today",
                              tm_next.tm_year == tm_now.tm_year && tm_next.tm_yday == tm_now.tm_yday);
    }

    cJSON *pulse_times = cJSON_CreateArray();
    if (s_settings.mode == LAMP_MODE_PULSE && s_settings.enabled) {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        int now_sec = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;

#define PAST_COUNT 2
#define FUTURE_COUNT 5
#define NEAREST_COUNT (PAST_COUNT + FUTURE_COUNT)
        int starts[NEAREST_COUNT];
        int days[NEAREST_COUNT];
        int n = lamp_pulse_nearest_starts(s_settings.pulse_interval_min,
                                          s_settings.pulse_duration_sec,
                                          now_sec, PAST_COUNT, FUTURE_COUNT,
                                          starts, days, NEAREST_COUNT);
        for (int i = 0; i < n; i++) {
            cJSON *item = cJSON_CreateObject();
            char buf[6];
            hhmm_min_to_str(starts[i] / 60, buf, sizeof(buf));
            cJSON_AddStringToObject(item, "time", buf);
            cJSON_AddNumberToObject(item, "day_offset", days[i]);
            cJSON_AddBoolToObject(item, "past", i < PAST_COUNT);
            cJSON_AddItemToArray(pulse_times, item);
        }
#undef PAST_COUNT
#undef FUTURE_COUNT
#undef NEAREST_COUNT
    }
    cJSON_AddItemToObject(root, "pulse_times", pulse_times);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return send_json(req, json);
}

static esp_err_t handle_post_water_now(httpd_req_t *req)
{
    if (lamp_pulse_now() != ESP_OK) {
        return reject_bad_request(req, "полив сейчас доступен только в импульсном режиме");
    }
    ESP_LOGI(TAG, "ручной полив запущен через веб");
    return handle_get_status(req);
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
    const cJSON *name = cJSON_GetObjectItem(root, "name");
    if (cJSON_IsString(name)) {
        if (strlen(name->valuestring) >= sizeof(next.name)) {
            cJSON_Delete(root);
            return reject_bad_request(req, "name: слишком длинное имя");
        }
        strlcpy(next.name, name->valuestring, sizeof(next.name));
    }

    const cJSON *mode = cJSON_GetObjectItem(root, "mode");
    if (cJSON_IsString(mode)) {
        if (strcmp(mode->valuestring, "schedule") == 0) {
            next.mode = LAMP_MODE_SCHEDULE;
        } else if (strcmp(mode->valuestring, "pulse") == 0) {
            next.mode = LAMP_MODE_PULSE;
        } else {
            cJSON_Delete(root);
            return reject_bad_request(req, "mode: ожидается schedule или pulse");
        }
    }
    const cJSON *p_int = cJSON_GetObjectItem(root, "pulse_interval_min");
    if (cJSON_IsNumber(p_int)) {
        // Диапазон как в Kconfig (LAMP_PULSE_INTERVAL_MIN)
        if (p_int->valueint < 1 || p_int->valueint > 1440) {
            cJSON_Delete(root);
            return reject_bad_request(req, "pulse_interval_min: допустимо 1–1440");
        }
        next.pulse_interval_min = p_int->valueint;
    }
    const cJSON *p_dur = cJSON_GetObjectItem(root, "pulse_duration_sec");
    if (cJSON_IsNumber(p_dur)) {
        // Диапазон как в Kconfig (LAMP_PULSE_DURATION_SEC)
        if (p_dur->valueint < 1 || p_dur->valueint > 600) {
            cJSON_Delete(root);
            return reject_bad_request(req, "pulse_duration_sec: допустимо 1–600");
        }
        next.pulse_duration_sec = p_dur->valueint;
    }
    if (next.pulse_duration_sec >= next.pulse_interval_min * 60) {
        cJSON_Delete(root);
        return reject_bad_request(req, "pulse_duration_sec должен быть меньше периода");
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
    mdns_discovery_update_mode(next.mode);
    ESP_LOGI(TAG, "настройки обновлены через веб: режим %s, %02d:%02d–%02d:%02d, "
                  "импульсы %d мин/%d с, gpio %d, %s",
             next.mode == LAMP_MODE_PULSE ? "pulse" : "schedule",
             next.on_min / 60, next.on_min % 60,
             next.off_min / 60, next.off_min % 60,
             next.pulse_interval_min, next.pulse_duration_sec,
             next.gpio, next.enabled ? "включено" : "выключено");

    return send_json(req, settings_to_json(&s_settings));
}

static const char *ota_state_to_str(ota_state_t state)
{
    switch (state) {
    case OTA_STATE_IDLE: return "idle";
    case OTA_STATE_CHECKING: return "checking";
    case OTA_STATE_UPDATE_AVAILABLE: return "update_available";
    case OTA_STATE_DOWNLOADING: return "downloading";
    case OTA_STATE_REBOOT_PENDING: return "reboot_pending";
    case OTA_STATE_UP_TO_DATE: return "up_to_date";
    case OTA_STATE_ERROR: return "error";
    default: return "unknown";
    }
}

static esp_err_t handle_get_update_status(httpd_req_t *req)
{
    ota_status_t st;
    ota_update_get_status(&st);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "current_version", st.current_version);
    cJSON_AddStringToObject(root, "available_version", st.available_version);
    cJSON_AddStringToObject(root, "state", ota_state_to_str(st.state));
    cJSON_AddStringToObject(root, "error", st.error_message);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return send_json(req, json);
}

static esp_err_t handle_post_update_check(httpd_req_t *req)
{
    ESP_LOGI(TAG, "ручная проверка обновления через веб");
    esp_err_t err = ota_update_check_now();
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "OTA не настроена");
    }
    return handle_get_update_status(req);
}

static esp_err_t handle_post_update_start(httpd_req_t *req)
{
    ESP_LOGI(TAG, "ручной запуск обновления через веб");
    esp_err_t err = ota_update_start_download();
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "обновление недоступно или уже выполняется");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "started");
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return send_json(req, json);
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
        { .uri = "/api/peers", .method = HTTP_GET, .handler = handle_get_peers },
        { .uri = "/api/settings", .method = HTTP_GET, .handler = handle_get_settings },
        { .uri = "/api/settings", .method = HTTP_POST, .handler = handle_post_settings },
        { .uri = "/api/status", .method = HTTP_GET, .handler = handle_get_status },
        { .uri = "/api/water-now", .method = HTTP_POST, .handler = handle_post_water_now },
        { .uri = "/api/update/status", .method = HTTP_GET, .handler = handle_get_update_status },
        { .uri = "/api/update/check", .method = HTTP_POST, .handler = handle_post_update_check },
        { .uri = "/api/update/start", .method = HTTP_POST, .handler = handle_post_update_start },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }

    ESP_LOGI(TAG, "веб-интерфейс запущен на порту %d", config.server_port);
}
