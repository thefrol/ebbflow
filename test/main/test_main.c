// Host-тесты чистой логики лампы (Unity, цель linux).
// Гоняются на компьютере: idf.py set-target linux && idf.py build
// && ./build/lamp_host_tests.elf

#include <stdlib.h>

#include "unity.h"

#include "hhmm.h"
#include "schedule.h"

TEST_CASE("hhmm_to_min: валидные строки", "[hhmm]")
{
    int min = -1;
    TEST_ASSERT_TRUE(hhmm_to_min("00:00", &min));
    TEST_ASSERT_EQUAL_INT(0, min);
    TEST_ASSERT_TRUE(hhmm_to_min("06:30", &min));
    TEST_ASSERT_EQUAL_INT(6 * 60 + 30, min);
    TEST_ASSERT_TRUE(hhmm_to_min("23:59", &min));
    TEST_ASSERT_EQUAL_INT(23 * 60 + 59, min);
    TEST_ASSERT_TRUE(hhmm_to_min("6:05", &min)); // без ведущего нуля тоже ок
    TEST_ASSERT_EQUAL_INT(6 * 60 + 5, min);
}

TEST_CASE("hhmm_to_min: невалидные строки отклоняются", "[hhmm]")
{
    int min = 0;
    TEST_ASSERT_FALSE(hhmm_to_min("24:00", &min)); // час за границей
    TEST_ASSERT_FALSE(hhmm_to_min("12:60", &min)); // минута за границей
    TEST_ASSERT_FALSE(hhmm_to_min("-1:30", &min));
    TEST_ASSERT_FALSE(hhmm_to_min("12", &min));    // нет минут
    TEST_ASSERT_FALSE(hhmm_to_min("abc", &min));
    TEST_ASSERT_FALSE(hhmm_to_min("", &min));
}

TEST_CASE("hhmm_min_to_str: форматирование с ведущими нулями", "[hhmm]")
{
    char buf[6];
    hhmm_min_to_str(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("00:00", buf);
    hhmm_min_to_str(6 * 60 + 5, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("06:05", buf);
    hhmm_min_to_str(23 * 60 + 59, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("23:59", buf);
}

TEST_CASE("hhmm: round-trip строка -> минуты -> строка", "[hhmm]")
{
    int min = 0;
    char buf[6];
    TEST_ASSERT_TRUE(hhmm_to_min("13:47", &min));
    hhmm_min_to_str(min, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("13:47", buf);
}

TEST_CASE("schedule: обычное расписание внутри суток", "[schedule]")
{
    // 06:00–23:00
    TEST_ASSERT_FALSE(lamp_schedule_active(360, 1380, 359));  // 05:59 до включения
    TEST_ASSERT_TRUE(lamp_schedule_active(360, 1380, 360));   // 06:00 ровно — уже свет
    TEST_ASSERT_TRUE(lamp_schedule_active(360, 1380, 1000));  // середина дня
    TEST_ASSERT_FALSE(lamp_schedule_active(360, 1380, 1380)); // 23:00 ровно — уже тьма
    TEST_ASSERT_FALSE(lamp_schedule_active(360, 1380, 0));    // полночь
}

TEST_CASE("schedule: расписание через полночь", "[schedule]")
{
    // 20:00–08:00
    TEST_ASSERT_TRUE(lamp_schedule_active(1200, 480, 1200)); // 20:00 включение
    TEST_ASSERT_TRUE(lamp_schedule_active(1200, 480, 1439)); // 23:59
    TEST_ASSERT_TRUE(lamp_schedule_active(1200, 480, 0));    // полночь — свет горит
    TEST_ASSERT_TRUE(lamp_schedule_active(1200, 480, 479));  // 07:59
    TEST_ASSERT_FALSE(lamp_schedule_active(1200, 480, 480)); // 08:00 выключение
    TEST_ASSERT_FALSE(lamp_schedule_active(1200, 480, 720)); // полдень
}

TEST_CASE("schedule: вырожденное расписание — свет всегда выключен", "[schedule]")
{
    for (int now = 0; now < 24 * 60; now += 60) {
        TEST_ASSERT_FALSE(lamp_schedule_active(600, 600, now));
    }
}

TEST_CASE("pulse: границы импульса (180 мин / 15 с)", "[pulse]")
{
    // импульсы в 00:00, 03:00, 06:00, ...
    int t_0300 = 3 * 3600;
    TEST_ASSERT_TRUE(lamp_pulse_active(180, 15, t_0300));         // 03:00:00 старт
    TEST_ASSERT_TRUE(lamp_pulse_active(180, 15, t_0300 + 14));    // 03:00:14 ещё идёт
    TEST_ASSERT_FALSE(lamp_pulse_active(180, 15, t_0300 + 15));   // 03:00:15 кончился
    TEST_ASSERT_FALSE(lamp_pulse_active(180, 15, t_0300 - 1));    // 02:59:59 ещё нет
    TEST_ASSERT_FALSE(lamp_pulse_active(180, 15, t_0300 + 3600)); // середина интервала
}

TEST_CASE("pulse: привязка к полуночи", "[pulse]")
{
    TEST_ASSERT_TRUE(lamp_pulse_active(180, 15, 0));              // 00:00:00 — начало суток
    TEST_ASSERT_TRUE(lamp_pulse_active(180, 15, 21 * 3600));      // 21:00 кратно 3 ч
    TEST_ASSERT_FALSE(lamp_pulse_active(180, 15, 21 * 3600 + 15));
}

TEST_CASE("pulse: интервал, не делящий сутки — фаза от полуночи", "[pulse]")
{
    // 250 мин: импульсы в 00:00, 04:10, 08:20, 12:30, 16:40, 20:50
    TEST_ASSERT_TRUE(lamp_pulse_active(250, 15, 250 * 60));
    TEST_ASSERT_TRUE(lamp_pulse_active(250, 15, 2 * 250 * 60));
    TEST_ASSERT_FALSE(lamp_pulse_active(250, 15, 3 * 3600)); // 03:00 — не начало интервала
}

TEST_CASE("pulse: вырожденные параметры — всегда выключено", "[pulse]")
{
    TEST_ASSERT_FALSE(lamp_pulse_active(0, 15, 0));   // нулевой интервал
    TEST_ASSERT_FALSE(lamp_pulse_active(180, 0, 0));  // нулевая длительность
    TEST_ASSERT_FALSE(lamp_pulse_active(1, 60, 0));   // длительность = периоду
    TEST_ASSERT_FALSE(lamp_pulse_active(1, 120, 30)); // длительность > периода
}

TEST_CASE("pulse_next_start: границы и привязка к полуночи", "[pulse]")
{
    // 180 мин / 15 с: импульсы в 00:00, 03:00, 06:00, ...
    int t_0300 = 3 * 3600;
    int t_0600 = 6 * 3600;
    TEST_ASSERT_EQUAL_INT(t_0600, lamp_pulse_next_start(180, 15, t_0300));       // ровно старт → следующий
    TEST_ASSERT_EQUAL_INT(t_0600, lamp_pulse_next_start(180, 15, t_0300 + 5));  // внутри импульса → следующий
    TEST_ASSERT_EQUAL_INT(t_0300, lamp_pulse_next_start(180, 15, t_0300 - 1));  // за секунду до старта
    TEST_ASSERT_EQUAL_INT(t_0300, lamp_pulse_next_start(180, 15, 1));           // 00:00:01 → 03:00
}

TEST_CASE("pulse_next_start: интервал, не делящий сутки", "[pulse]")
{
    // 250 мин: 00:00, 04:10, 08:20, 12:30, 16:40, 20:50, 01:00, 05:10, ...
    TEST_ASSERT_EQUAL_INT(250 * 60, lamp_pulse_next_start(250, 15, 1));              // 00:00:01
    TEST_ASSERT_EQUAL_INT(3 * 250 * 60, lamp_pulse_next_start(250, 15, 2 * 250 * 60 + 10)); // внутри импульса 08:20
    TEST_ASSERT_EQUAL_INT(6 * 250 * 60, lamp_pulse_next_start(250, 15, 20 * 3600 + 50 * 60 + 30)); // после 20:50 → 01:00
}

TEST_CASE("pulse_next_start: невалидные параметры", "[pulse]")
{
    TEST_ASSERT_EQUAL_INT(-1, lamp_pulse_next_start(0, 15, 0));
    TEST_ASSERT_EQUAL_INT(-1, lamp_pulse_next_start(180, 0, 0));
    TEST_ASSERT_EQUAL_INT(-1, lamp_pulse_next_start(1, 60, 0));
    TEST_ASSERT_EQUAL_INT(-1, lamp_pulse_next_start(1, 120, 30));
}

TEST_CASE("pulse_nearest_starts: обычный случай", "[pulse]")
{
    // 180 мин / 15 с, сейчас 10:00:00 — импульсы в ..., 06:00, 09:00, 12:00, ...
    int starts[7];
    int days[7];
    int n = lamp_pulse_nearest_starts(180, 15, 10 * 3600, 2, 5, starts, days, 7);
    TEST_ASSERT_EQUAL_INT(7, n);
    // прошедшие: 09:00, 06:00
    TEST_ASSERT_EQUAL_INT(9 * 3600, starts[0]);
    TEST_ASSERT_EQUAL_INT(0, days[0]);
    TEST_ASSERT_EQUAL_INT(6 * 3600, starts[1]);
    TEST_ASSERT_EQUAL_INT(0, days[1]);
    // будущие: 12:00, 15:00, 18:00, 21:00, 00:00
    TEST_ASSERT_EQUAL_INT(12 * 3600, starts[2]);
    TEST_ASSERT_EQUAL_INT(0, days[2]);
    TEST_ASSERT_EQUAL_INT(15 * 3600, starts[3]);
    TEST_ASSERT_EQUAL_INT(0, days[3]);
    TEST_ASSERT_EQUAL_INT(18 * 3600, starts[4]);
    TEST_ASSERT_EQUAL_INT(0, days[4]);
    TEST_ASSERT_EQUAL_INT(21 * 3600, starts[5]);
    TEST_ASSERT_EQUAL_INT(0, days[5]);
    TEST_ASSERT_EQUAL_INT(0, starts[6]);
    TEST_ASSERT_EQUAL_INT(1, days[6]);
}

TEST_CASE("pulse_nearest_starts: внутри импульса", "[pulse]")
{
    // 180 мин / 15 с, сейчас 09:00:05 (внутри импульса 09:00–09:00:15)
    int starts[3];
    int days[3];
    int n = lamp_pulse_nearest_starts(180, 15, 9 * 3600 + 5, 1, 2, starts, days, 3);
    TEST_ASSERT_EQUAL_INT(3, n);
    // ближайший прошедший — 06:00 (текущий ещё идёт)
    TEST_ASSERT_EQUAL_INT(6 * 3600, starts[0]);
    TEST_ASSERT_EQUAL_INT(0, days[0]);
    // будущие: 12:00, 15:00
    TEST_ASSERT_EQUAL_INT(12 * 3600, starts[1]);
    TEST_ASSERT_EQUAL_INT(0, days[1]);
    TEST_ASSERT_EQUAL_INT(15 * 3600, starts[2]);
    TEST_ASSERT_EQUAL_INT(0, days[2]);
}

TEST_CASE("pulse_nearest_starts: переход через полночь", "[pulse]")
{
    // 180 мин / 15 с, сейчас 23:55:00
    int starts[4];
    int days[4];
    int n = lamp_pulse_nearest_starts(180, 15, 23 * 3600 + 55 * 60, 2, 2, starts, days, 4);
    TEST_ASSERT_EQUAL_INT(4, n);
    // прошедшие: 21:00, 18:00
    TEST_ASSERT_EQUAL_INT(21 * 3600, starts[0]);
    TEST_ASSERT_EQUAL_INT(0, days[0]);
    TEST_ASSERT_EQUAL_INT(18 * 3600, starts[1]);
    TEST_ASSERT_EQUAL_INT(0, days[1]);
    // будущие: 00:00 (завтра), 03:00 (завтра)
    TEST_ASSERT_EQUAL_INT(0, starts[2]);
    TEST_ASSERT_EQUAL_INT(1, days[2]);
    TEST_ASSERT_EQUAL_INT(3 * 3600, starts[3]);
    TEST_ASSERT_EQUAL_INT(1, days[3]);
}

TEST_CASE("pulse_nearest_starts: интервал 170 мин", "[pulse]")
{
    // 170 мин: импульсы в 00:00, 02:50, 05:40, 08:30, 11:20, 14:10, 17:00, 19:50, 22:40, 01:30, ...
    int starts[3];
    int days[3];
    // сейчас 12:00:00 — последний прошедший 11:20, следующий 14:10
    int n = lamp_pulse_nearest_starts(170, 15, 12 * 3600, 1, 2, starts, days, 3);
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_INT(11 * 3600 + 20 * 60, starts[0]);
    TEST_ASSERT_EQUAL_INT(0, days[0]);
    TEST_ASSERT_EQUAL_INT(14 * 3600 + 10 * 60, starts[1]);
    TEST_ASSERT_EQUAL_INT(0, days[1]);
    TEST_ASSERT_EQUAL_INT(17 * 3600, starts[2]);
    TEST_ASSERT_EQUAL_INT(0, days[2]);
}

TEST_CASE("pulse_nearest_starts: невалидные параметры", "[pulse]")
{
    int starts[4];
    int days[4];
    TEST_ASSERT_EQUAL_INT(0, lamp_pulse_nearest_starts(0, 15, 0, 1, 1, starts, days, 4));
    TEST_ASSERT_EQUAL_INT(0, lamp_pulse_nearest_starts(180, 0, 0, 1, 1, starts, days, 4));
    TEST_ASSERT_EQUAL_INT(0, lamp_pulse_nearest_starts(1, 60, 0, 1, 1, starts, days, 4));
    TEST_ASSERT_EQUAL_INT(0, lamp_pulse_nearest_starts(180, 15, -1, 1, 1, starts, days, 4));
    TEST_ASSERT_EQUAL_INT(0, lamp_pulse_nearest_starts(180, 15, 24 * 3600, 1, 1, starts, days, 4));
    TEST_ASSERT_EQUAL_INT(0, lamp_pulse_nearest_starts(180, 15, 0, 0, 0, NULL, NULL, 0));
}

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    int failures = UNITY_END();
    // linux-цель: без явного выхода симулятор FreeRTOS крутится вечно
    exit(failures);
}
