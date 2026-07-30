# Тестируем только чистые модули прошивки (без зависимостей от ESP-IDF),
# компилируя их напрямую — целиком main на linux-цели не собрать.
idf_component_register(
    SRCS "test_main.c" "../../main/schedule.c" "../../main/hhmm.c"
    INCLUDE_DIRS "../../main"
    REQUIRES unity
)
