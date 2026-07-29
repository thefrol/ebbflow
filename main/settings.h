#pragma once

#include <stdbool.h>

#include "esp_err.h"

// Настройки лампы. Дефолты приходят из Kconfig (menuconfig),
// при первом запуске сбрасываются в NVS и дальше живут там —
// их можно будет менять на лету (веб-интерфейс, MQTT) без перепрошивки.
typedef struct {
    int on_min;   // минуты от полуночи: включение
    int off_min;  // минуты от полуночи: выключение
    int gpio;     // выходной пин лампы
    bool enabled; // расписание активно
} lamp_settings_t;

// Загружает настройки из NVS. При первом запуске (ключей нет)
// записывает в NVS дефолты из menuconfig.
esp_err_t settings_load(lamp_settings_t *out);

// Сохраняет настройки в NVS.
esp_err_t settings_save(const lamp_settings_t *settings);
