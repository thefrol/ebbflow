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

void app_main(void)
{
    UNITY_BEGIN();
    unity_run_all_tests();
    int failures = UNITY_END();
    // linux-цель: без явного выхода симулятор FreeRTOS крутится вечно
    exit(failures);
}
