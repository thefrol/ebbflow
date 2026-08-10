#pragma once

#include "esp_err.h"
#include "settings.h"

#include <stdbool.h>
#include <time.h>

// Настраивает пин лампы и запоминает начальные настройки.
esp_err_t lamp_init(const lamp_settings_t *settings);

// Запускает фоновую задачу, которая применяет расписание.
// Вызывать после синхронизации времени (time_sync_start).
void lamp_start(void);

// Подменяет настройки на лету (для веб-интерфейса/MQTT).
// Безопасно вызывать из другой задачи.
void lamp_apply_settings(const lamp_settings_t *settings);

// Возвращает UTC-время начала следующего импульса, или (time_t)-1, если
// режим не импульсный, лампа выключена или параметры невалидны.
time_t lamp_next_pulse_time(void);

// Запускает ручной импульс длительности pulse_duration_sec.
// Возвращает ESP_OK только в режиме LAMP_MODE_PULSE и при enabled.
esp_err_t lamp_pulse_now(void);

// Идёт ли сейчас ручной импульс, запущенный через lamp_pulse_now.
bool lamp_manual_pulse_active(void);
