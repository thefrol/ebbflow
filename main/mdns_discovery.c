#include "mdns_discovery.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "mdns.h"
#include "sdkconfig.h"

static const char *TAG = "mdns";

static char s_self_id[MDNS_PEER_ID_LEN];
static char s_self_name[MDNS_PEER_NAME_LEN];
static char s_self_host[MDNS_PEER_HOST_LEN];

// Формирует id из MAC (6 байт -> 12 hex + '\0').
static void mac_to_id_string(char *out, size_t out_len)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, out_len, "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Формирует уникальный hostname: если имя уже задано пользователем — без суффикса,
// иначе добавляем MAC, чтобы новые/дефолтные устройства не конфликтовали.
static void build_self_host(const lamp_settings_t *settings)
{
    if (settings->name_customized && settings->name[0] != '\0') {
        strlcpy(s_self_host, settings->name, sizeof(s_self_host));
    } else {
        snprintf(s_self_host, sizeof(s_self_host), "%s-%s", settings->name, s_self_id);
    }
}

// mode -> строка для TXT.
static const char *mode_to_str(lamp_mode_t mode)
{
    return mode == LAMP_MODE_PULSE ? "pulse" : "schedule";
}

// Безопасно копирует строку из TXT в буфер.
static void copy_txt_value(char *out, size_t out_len, const char *value)
{
    if (!value) {
        out[0] = '\0';
        return;
    }
    size_t len = strlen(value);
    if (len >= out_len) {
        len = out_len - 1;
    }
    memcpy(out, value, len);
    out[len] = '\0';
}

// Ищет TXT-запись по ключу в результате.
static const char *find_txt(mdns_txt_item_t *txt, uint8_t txt_count, const char *key)
{
    for (int i = 0; i < txt_count; i++) {
        if (txt[i].key && strcmp(txt[i].key, key) == 0) {
            return txt[i].value ? txt[i].value : "";
        }
    }
    return NULL;
}

esp_err_t mdns_discovery_init(const lamp_settings_t *settings)
{
    mac_to_id_string(s_self_id, sizeof(s_self_id));
    strlcpy(s_self_name, settings->name, sizeof(s_self_name));
    build_self_host(settings);

    // Один hostname для mDNS и DHCP — меньше путаницы в сети.
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != NULL) {
        esp_netif_set_hostname(netif, s_self_host);
    }

    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mdns_hostname_set(s_self_host);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_hostname_set failed: %s", esp_err_to_name(err));
        return err;
    }

    err = mdns_instance_name_set(s_self_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_instance_name_set failed: %s", esp_err_to_name(err));
        return err;
    }

    const char *mode = mode_to_str(settings->mode);
    const char *version = esp_app_get_description()->version;

    mdns_txt_item_t txt[] = {
        { .key = "id",   .value = s_self_id },
        { .key = "ver",  .value = version },
        { .key = "chip", .value = CONFIG_IDF_TARGET },
        { .key = "mode", .value = mode },
    };

    err = mdns_service_add(NULL, "_ebbflow", "_tcp", 80, txt, sizeof(txt) / sizeof(txt[0]));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_service_add failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "mDNS запущен: host=%s, name=%s, сервис _ebbflow._tcp:%d, id=%s, ver=%s, chip=%s, mode=%s",
             s_self_host, s_self_name, 80, s_self_id, version, CONFIG_IDF_TARGET, mode);
    return ESP_OK;
}

esp_err_t mdns_discovery_update_mode(lamp_mode_t mode)
{
    const char *mode_str = mode_to_str(mode);
    esp_err_t err = mdns_service_txt_item_set("_ebbflow", "_tcp", "mode", mode_str);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "mdns_service_txt_item_set failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "TXT mode обновлён: %s", mode_str);
    return ESP_OK;
}

static bool peer_exists(const mdns_peer_t *peers, int count, const char *id)
{
    if (!id || id[0] == '\0') {
        return false;
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(peers[i].id, id) == 0) {
            return true;
        }
    }
    return false;
}

int mdns_discovery_browse(mdns_peer_t *peers, int max)
{
    if (max <= 0) {
        return 0;
    }

    int count = 0;
    // До 3 попыток: multicast на ESP32 часто теряется, особенно со включённым power save.
    // Останавливаемся раньше, если нашли всех возможных соседей.
    for (int attempt = 0; attempt < 3 && count < max; attempt++) {
        mdns_result_t *results = NULL;
        // Таймаут 3 с на попытку, max_results с запасом под себя.
        esp_err_t err = mdns_query_ptr("_ebbflow", "_tcp", 3000, (size_t)(max + 1), &results);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "browse попытка %d не удалась: %s", attempt + 1, esp_err_to_name(err));
            continue;
        }

        for (mdns_result_t *r = results; r && count < max; r = r->next) {
            if (!r->hostname || !r->addr) {
                continue;
            }

            // Фильтруем только собственное устройство по уникальному id (MAC).
            const char *id_txt = find_txt(r->txt, r->txt_count, "id");
            if (id_txt && strcmp(id_txt, s_self_id) == 0) {
                continue;
            }
            // Пропускаем дубликаты между попытками.
            if (peer_exists(peers, count, id_txt)) {
                continue;
            }

            mdns_peer_t *p = &peers[count];
            memset(p, 0, sizeof(*p));

            copy_txt_value(p->id, sizeof(p->id), id_txt);
            copy_txt_value(p->name, sizeof(p->name), r->instance_name ? r->instance_name : r->hostname);
            snprintf(p->host, sizeof(p->host), "%s.local", r->hostname);
            p->port = r->port;

            // IP из первого адреса — даём больше времени, чем раньше.
            esp_ip4_addr_t ip4_addr;
            esp_err_t ip_err = mdns_query_a(r->hostname, 1000, &ip4_addr);
            if (ip_err == ESP_OK) {
                snprintf(p->ip, sizeof(p->ip), IPSTR, IP2STR(&ip4_addr));
            } else {
                // Fallback: пробуем адрес из результата ptr.
                if (r->addr && r->addr->addr.type == ESP_IPADDR_TYPE_V4) {
                    esp_ip4_addr_t ip4 = r->addr->addr.u_addr.ip4;
                    snprintf(p->ip, sizeof(p->ip), IPSTR, IP2STR(&ip4));
                } else {
                    p->ip[0] = '\0';
                }
            }

            copy_txt_value(p->version, sizeof(p->version), find_txt(r->txt, r->txt_count, "ver"));
            copy_txt_value(p->chip, sizeof(p->chip), find_txt(r->txt, r->txt_count, "chip"));
            copy_txt_value(p->mode, sizeof(p->mode), find_txt(r->txt, r->txt_count, "mode"));

            count++;
            ESP_LOGI(TAG, "найден сосед (попытка %d): %s (%s) at %s:%d ver=%s chip=%s mode=%s",
                     attempt + 1, p->name, p->id, p->host, p->port, p->version, p->chip, p->mode);
        }

        mdns_query_results_free(results);
    }

    ESP_LOGI(TAG, "browse завершён: найдено %d соседей", count);
    return count;
}
