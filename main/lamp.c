#include "lamp.h"

#include <stdbool.h>
#include <time.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lamp";

static lamp_settings_t s_settings;
static int s_state = -1; // текущий уровень на пине; -1 — ещё не применялся

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
    gpio_drive(s_settings.gpio, 0);
    s_state = 0;
    ESP_LOGI(TAG, "пин GPIO%d, расписание %02d:%02d — %02d:%02d%s",
             s_settings.gpio,
             s_settings.on_min / 60, s_settings.on_min % 60,
             s_settings.off_min / 60, s_settings.off_min % 60,
             s_settings.enabled ? "" : " (выключено)");
    return ESP_OK;
}

void lamp_apply_settings(const lamp_settings_t *settings)
{
    if (settings->gpio != s_settings.gpio) {
        gpio_drive(settings->gpio, 0);
    }
    s_settings = *settings;
    s_state = -1; // следующий тик задачи применит состояние заново
}

static bool schedule_active(int on_min, int off_min, int now_min)
{
    if (on_min == off_min) {
        return false; // вырожденное расписание — свет выключен
    }
    if (on_min < off_min) {
        return now_min >= on_min && now_min < off_min;
    }
    // расписание идёт через полночь
    return now_min >= on_min || now_min < off_min;
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
        int now_min = tm_now.tm_hour * 60 + tm_now.tm_min;

        desired = (s_settings.enabled &&
                   schedule_active(s_settings.on_min, s_settings.off_min, now_min))
                      ? 1
                      : 0;
#endif

        if (desired != s_state) {
#ifdef CONFIG_LAMP_DIAG_BLINK
            ESP_LOGI(TAG, "мигание: %s", desired ? "ВКЛ" : "ВЫКЛ");
#else
            time_t now = time(NULL);
            struct tm tm_now;
            localtime_r(&now, &tm_now);
            ESP_LOGI(TAG, "%02d:%02d — свет %s",
                     tm_now.tm_hour, tm_now.tm_min, desired ? "ВКЛ" : "ВЫКЛ");
#endif
            gpio_drive(s_settings.gpio, desired);
            s_state = desired;
        }

        vTaskDelay(delay);
    }
}

void lamp_start(void)
{
    xTaskCreate(lamp_task, "lamp", 3072, NULL, 5, NULL);
}
