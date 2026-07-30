#pragma once

#include <stdbool.h>
#include <stddef.h>

// Разбор/формат времени "HH:MM" — граничный формат настроек
// (Kconfig, HTTP API). Внутри прошивка оперирует минутами от полуночи.
// Без зависимостей от ESP-IDF, чтобы покрываться host-тестами (test/).

// Разбирает строку "HH:MM" в минуты от полуночи.
// true — разобрали и значение валидно (0–23 ч, 0–59 мин).
bool hhmm_to_min(const char *s, int *out_min);

// Форматирует минуты от полуночи в "HH:MM" (buf минимум 6 байт).
void hhmm_min_to_str(int min, char *buf, size_t len);
