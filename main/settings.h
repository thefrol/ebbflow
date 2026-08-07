#pragma once

#include <stdbool.h>

#include "esp_err.h"

// Настройки лампы. Дефолты приходят из Kconfig (menuconfig),
// при первом запуске сбрасываются в NVS и дальше живут там —
// их можно будет менять на лету (веб-интерфейс, MQTT) без перепрошивки.
// Режим работы выхода.
typedef enum {
    LAMP_MODE_SCHEDULE = 0, // вкл/выкл по времени суток (свет)
    LAMP_MODE_PULSE = 1,    // короткие импульсы по таймеру (полив)
} lamp_mode_t;

typedef struct {
    lamp_mode_t mode;      // режим: расписание или импульсы
    int on_min;            // минуты от полуночи: включение (режим schedule)
    int off_min;           // минуты от полуночи: выключение (режим schedule)
    int pulse_interval_min; // период импульсов, минуты (режим pulse)
    int pulse_duration_sec; // длительность импульса, секунды (режим pulse)
    int gpio;              // выходной пин лампы
    bool enabled;          // расписание активно
} lamp_settings_t;

// Загружает настройки из NVS. При первом запуске (ключей нет)
// записывает в NVS дефолты из menuconfig.
esp_err_t settings_load(lamp_settings_t *out);

// Сохраняет настройки в NVS.
esp_err_t settings_save(const lamp_settings_t *settings);
