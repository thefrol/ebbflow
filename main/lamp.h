#pragma once

#include "esp_err.h"
#include "settings.h"

// Настраивает пин лампы и запоминает начальные настройки.
esp_err_t lamp_init(const lamp_settings_t *settings);

// Запускает фоновую задачу, которая применяет расписание.
// Вызывать после синхронизации времени (time_sync_start).
void lamp_start(void);

// Подменяет настройки на лету (для будущего веб-интерфейса/MQTT).
// TODO: при появлении второго писателя добавить мьютекс.
void lamp_apply_settings(const lamp_settings_t *settings);
