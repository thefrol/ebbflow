#pragma once

#include <stdint.h>

#include "esp_err.h"

#include "settings.h"

#define MDNS_PEERS_MAX 8

// Информация о соседнем устройстве, найденном через mDNS.
#define MDNS_PEER_ID_LEN 32
#define MDNS_PEER_NAME_LEN 32
#define MDNS_PEER_HOST_LEN 64
#define MDNS_PEER_IP_LEN 16
#define MDNS_PEER_VERSION_LEN 16
#define MDNS_PEER_CHIP_LEN 16
#define MDNS_PEER_MODE_LEN 16

typedef struct {
    char id[MDNS_PEER_ID_LEN];           // уникальный ID (MAC в hex)
    char name[MDNS_PEER_NAME_LEN];       // mDNS hostname / имя устройства
    char host[MDNS_PEER_HOST_LEN];       // hostname.local
    char ip[MDNS_PEER_IP_LEN];           // IPv4 строкой (fallback)
    uint16_t port;                       // TCP-порт сервиса
    char version[MDNS_PEER_VERSION_LEN]; // версия прошивки из TXT
    char chip[MDNS_PEER_CHIP_LEN];       // chip target из TXT
    char mode[MDNS_PEER_MODE_LEN];       // режим из TXT (schedule/pulse)
} mdns_peer_t;

// Запускает mDNS: инициализация, hostname, instance name, сервис _ebbflow._tcp:80.
// Вызывать после получения IP (после wifi_setup_connect).
esp_err_t mdns_discovery_init(const lamp_settings_t *settings);

// Обновляет TXT-запись mode при смене режима без перезапуска сервиса.
esp_err_t mdns_discovery_update_mode(lamp_mode_t mode);

// Ищет соседние устройства через mDNS browse. Таймаут ~2.5 с.
// peers — буфер, max — его размер. Возвращает количество найденных (0..max).
// Собственное устройство отфильтровывается.
// Блокирующий: используется фоновой задачей, а не HTTP-обработчиком.
int mdns_discovery_browse(mdns_peer_t *peers, int max);

// Возвращает последний закэшированный список соседей (копия из фоновой задачи).
// Быстрая, не блокирует HTTP-обработчик. peers — буфер, max — размер.
// Возвращает количество записей (0..max).
int mdns_discovery_peers_get(mdns_peer_t *peers, int max);
