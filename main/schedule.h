#pragma once

#include <stdbool.h>

// Чистая логика расписания (без зависимостей от ESP-IDF) —
// вынесена, чтобы покрываться host-тестами (test/, цель linux).

// Активно ли расписание в момент now_min (все значения — минуты от полуночи).
// on_min == off_min — вырожденное расписание, свет всегда выключен.
// on_min > off_min — расписание идёт через полночь.
bool lamp_schedule_active(int on_min, int off_min, int now_min);

// Активен ли импульс в момент now_sec (секунды от полуночи).
// Интервалы привязаны к полуночи: начало каждого кратно interval_min минутам,
// импульс длится duration_sec секунд от начала интервала.
// Невалидные параметры или duration_sec >= interval*60 — выключено
// (защита от вечно включённого пина).
bool lamp_pulse_active(int interval_min, int duration_sec, int now_sec);

// Секунды от полуночи до начала следующего импульса (не текущего).
// Возвращает -1 при невалидных параметрах.
// Если now_sec попадает в импульс, возвращает старт следующего интервала.
int lamp_pulse_next_start(int interval_min, int duration_sec, int now_sec);

// Заполняет буферы out_starts и out_day_offsets ближайшими импульсами.
// past_count — сколько уже завершившихся импульсов включить (<= max_count).
// future_count — сколько ещё не начавшихся импульсов включить (<= max_count).
// out_starts[k] — секунды от полуночи дня out_day_offsets[k].
// out_day_offsets[k] — смещение дня относительно текущих суток
//                      (0 = сегодня, -1 = вчера, 1 = завтра и т.д.).
// Возвращает количество записанных элементов или 0 при невалидных параметрах.
int lamp_pulse_nearest_starts(int interval_min, int duration_sec, int now_sec,
                              int past_count, int future_count,
                              int *out_starts, int *out_day_offsets, int max_count);
