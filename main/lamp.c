#include "lamp.h"

#include <stdbool.h>
#include <time.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "schedule.h"

static const char *TAG = "lamp";

static lamp_settings_t s_settings;
static int s_state = -1;          // текущий уровень на пине; -1 — ещё не применялся
static time_t s_manual_pulse_end; // UTC-время окончания ручного импульса
static SemaphoreHandle_t s_mutex; // защищает s_settings и s_manual_pulse_end

static void gpio_drive(int gpio, int level)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(gpio, level));
}

esp_err_t lamp_init(const lamp_settings_t *settings)
{
    s_settings = *settings;
    s_manual_pulse_end = 0;
    s_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(s_mutex ? ESP_OK : ESP_FAIL);
    gpio_drive(s_settings.gpio, 0);
    s_state = 0;
    if (s_settings.mode == LAMP_MODE_PULSE) {
        ESP_LOGI(TAG, "пин GPIO%d, импульсы: каждые %d мин на %d с%s",
                 s_settings.gpio, s_settings.pulse_interval_min,
                 s_settings.pulse_duration_sec,
                 s_settings.enabled ? "" : " (выключено)");
    } else {
        ESP_LOGI(TAG, "пин GPIO%d, расписание %02d:%02d — %02d:%02d%s",
                 s_settings.gpio,
                 s_settings.on_min / 60, s_settings.on_min % 60,
                 s_settings.off_min / 60, s_settings.off_min % 60,
                 s_settings.enabled ? "" : " (выключено)");
    }
    return ESP_OK;
}

void lamp_apply_settings(const lamp_settings_t *settings)
{
    if (settings->gpio != s_settings.gpio) {
        gpio_drive(settings->gpio, 0);
    }
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        s_settings = *settings;
        s_manual_pulse_end = 0; // смена настроек сбрасывает ручной импульс
        xSemaphoreGive(s_mutex);
    }
    s_state = -1; // следующий тик задачи применит состояние заново
}

time_t lamp_next_pulse_time(void)
{
    lamp_settings_t settings = {0};
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        settings = s_settings;
        xSemaphoreGive(s_mutex);
    }
    if (settings.mode != LAMP_MODE_PULSE || !settings.enabled) {
        return (time_t)-1;
    }
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    int now_sec = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;

    int next_sec = lamp_pulse_next_start(settings.pulse_interval_min, settings.pulse_duration_sec, now_sec);
    if (next_sec < 0) {
        return (time_t)-1;
    }

    time_t midnight = now - now_sec;
    time_t next = midnight + next_sec;
    if (next <= now) {
        next += 24 * 3600;
    }
    return next;
}

esp_err_t lamp_pulse_now(void)
{
    lamp_settings_t settings = {0};
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        settings = s_settings;
        xSemaphoreGive(s_mutex);
    }
    if (settings.mode != LAMP_MODE_PULSE || !settings.enabled) {
        return ESP_FAIL;
    }
    time_t now = time(NULL);
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        s_manual_pulse_end = now + settings.pulse_duration_sec;
        xSemaphoreGive(s_mutex);
    }
    ESP_LOGI(TAG, "ручной полив на %d с", settings.pulse_duration_sec);
    return ESP_OK;
}

bool lamp_manual_pulse_active(void)
{
    time_t now = time(NULL);
    bool active = false;
    if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
        active = (s_manual_pulse_end > now);
        xSemaphoreGive(s_mutex);
    }
    return active;
}

static void lamp_task(void *arg)
{
    for (;;) {
        TickType_t delay = pdMS_TO_TICKS(15000);
        int desired;

#if defined(CONFIG_LAMP_DIAG_BLINK)
        // Диагностика железа: мигание 1 Гц, расписание игнорируется
        desired = (s_state == 1) ? 0 : 1;
        delay = pdMS_TO_TICKS(500);
#elif defined(CONFIG_LAMP_DIAG_FORCE_OFF)
        // Диагностика железа: пин жёстко в нуле, расписание игнорируется
        desired = 0;
#else
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);

        lamp_settings_t settings = {0};
        time_t manual_pulse_end = 0;
        if (xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE) {
            settings = s_settings;
            manual_pulse_end = s_manual_pulse_end;
            xSemaphoreGive(s_mutex);
        }

        if (manual_pulse_end > now) {
            desired = 1;
        } else if (settings.mode == LAMP_MODE_PULSE) {
            int now_sec = tm_now.tm_hour * 3600 + tm_now.tm_min * 60 + tm_now.tm_sec;
            desired = (settings.enabled &&
                       lamp_pulse_active(settings.pulse_interval_min,
                                         settings.pulse_duration_sec, now_sec))
                          ? 1
                          : 0;
            // 15-секундный тик съест короткий импульс — тикаем каждую секунду
            delay = pdMS_TO_TICKS(1000);
        } else {
            int now_min = tm_now.tm_hour * 60 + tm_now.tm_min;
            desired = (settings.enabled &&
                       lamp_schedule_active(settings.on_min, settings.off_min, now_min))
                          ? 1
                          : 0;
        }
#endif

        if (desired != s_state) {
#ifdef CONFIG_LAMP_DIAG_BLINK
            ESP_LOGI(TAG, "мигание: %s", desired ? "ВКЛ" : "ВЫКЛ");
#else
            time_t log_now = time(NULL);
            struct tm log_tm;
            localtime_r(&log_now, &log_tm);
            bool manual_active = (manual_pulse_end > log_now);
            ESP_LOGI(TAG, "%02d:%02d:%02d — %s %s%s",
                     log_tm.tm_hour, log_tm.tm_min, log_tm.tm_sec,
                     settings.mode == LAMP_MODE_PULSE ? "полив" : "свет",
                     desired ? "ВКЛ" : "ВЫКЛ",
                     manual_active ? " (ручной)" : "");
#endif
            gpio_drive(settings.gpio, desired);
            s_state = desired;
        }

        vTaskDelay(delay);
    }
}

void lamp_start(void)
{
    xTaskCreate(lamp_task, "lamp", 3072, NULL, 5, NULL);
}
