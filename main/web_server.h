#pragma once

#include "settings.h"

// Запускает HTTP-сервер (порт 80) со страничкой настроек и REST API.
// Вызывать после wifi_setup_connect() — сеть уже должна быть поднята.
void web_server_start(const lamp_settings_t *settings);
