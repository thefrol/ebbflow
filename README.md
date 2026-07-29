# ebbflow-lamp

Лампа для растений на ESP32-C3/C6 (ESP-IDF v6.0.2): включает и выключает свет
на одном пине по расписанию, время синхронизирует по NTP.

## Быстрый старт

```bash
. ~/esp/esp-idf/export.sh        # окружение ESP-IDF
idf.py set-target esp32c3        # или esp32c6
idf.py menuconfig                # раздел "Ebbflow Lamp": Wi-Fi, пин, время
idf.py build flash monitor
```

Настройки (расписание, пин) после первого запуска живут в NVS — дальше их
можно будет менять без перепрошивки (веб-интерфейс в планах, см. `ideas/`).

Подробности процесса и соглашения — в `AGENTS.md`.
