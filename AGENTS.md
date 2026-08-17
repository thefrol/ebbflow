# ebbflow-lamp — AGENTS.md

Таймер на ESP32-C3 / ESP32-C6 для одного пина (реле/драйвер/MOSFET). Два
режима, переключаемые через веб-интерфейс: **свет** по расписанию (вкл/выкл
по времени суток) и **полив** короткими импульсами (каждые N минут на S
секунд). Время берёт по NTP из интернета. Репозиторий — чистый
ESP-IDF проект, без PlatformIO и Arduino.

## Стек

- **ESP-IDF v6.0.2** — установлен в `~/esp/esp-idf` (shallow clone, тег `v6.0.2`).
- Цели: `esp32c3` и `esp32c6` (тулчейны установлены для обеих).
- Язык: C, FreeRTOS. Зависимостей вне ESP-IDF нет.
- Документация: MCP Context7, library ID
  `/websites/espressif_projects_esp-idf_en_release-v6_0` (официальные доки v6.0).
  Онлайн: <https://docs.espressif.com/projects/esp-idf/en/v6.0.2/>.

## Окружение и команды

Перед любыми `idf.py` в новой сессии shell:

```bash
. ~/esp/esp-idf/export.sh
```

Дальше из корня репозитория:

```bash
idf.py set-target esp32c3        # или esp32c6; переключение сносит sdkconfig
idf.py menuconfig                # Ebbflow Lamp: Wi-Fi, пин, расписание, TZ
idf.py build
idf.py -p /dev/cu.usbserial-XXX flash monitor   # порт уточнить по ls /dev/cu.*
```

ВАЖНО: расписание/режим/пин живут в NVS и сидятся из Kconfig только при пустом
NVS. Поменять их на прошитом устройстве проще всего через веб-интерфейс.
Чтобы применить новые дефолты из menuconfig на уже прошитом устройстве,
надо стереть NVS — надёжнее всего полностью:
`esptool --chip esp32c3 -p PORT erase-flash && idf.py -p PORT flash`
(точечный `erase-region` по адресу nvs у нас не сработал).

- `sdkconfig` в git не коммитим (там Wi-Fi пароль) — дефолты в
  `sdkconfig.defaults`, он в репозитории.
- Wi-Fi креды живут в `sdkconfig.defaults.local` (gitignored, не коммитить!).
  Применяется при создании sdkconfig (после set-target):
  `idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.local" set-target esp32c3`
  либо те же строки можно дописать прямо в сгенерированный `sdkconfig`.
- Разметка флеша: своя `partitions.csv`, два OTA-слота по ~1.9M без factory.
- OTA работает: устройство само опрашивает GitHub Releases и обновляется.
  **Полная документация — `docs/ota.md`**; скиллы `.agents/skills/ota`
  (архитектура и отладка) и `.agents/skills/firmware-release` (выпуск релиза).
  Релиз — тегом: `git tag v0.5.0 && git push origin v0.5.0` → CI
  (`.github/workflows/release.yml`) собирает обе цели в контейнере
  `espressif/idf:v6.0.2` и публикует релиз с `ebbflow-lamp-<chip>.bin`.
  Версия прошивки = тег (CI передаёт `-DPROJECT_VER`); не забывать поднимать
  `project(... VERSION x.y.z)` в корневом `CMakeLists.txt` под новый тег.
  Wi-Fi креды для CI-сборок — в GitHub Secrets (`WIFI_SSID`, `WIFI_PASSWORD`).
- Собирать надо под обе цели перед коммитом изменений в коде.
- Host-тесты чистой логики (без железа, цель `linux`, Unity): каталог
  `test/`, запуск `cd test && idf.py --preview set-target linux &&
  idf.py build && ./build/lamp_host_tests.elf`. Тестируемые модули
  (`hhmm`, `schedule`) специально не зависят от ESP-IDF — новую чистую
  логику выносить в такие же модули и покрывать там.

## Структура

- `main/main.c` — app_main: NVS → настройки → Wi-Fi → SNTP → задача лампы.
- `main/settings.*` — настройки в NVS (namespace `lamp`): режим
  (`LAMP_MODE_SCHEDULE`/`LAMP_MODE_PULSE`), расписание, параметры импульсов
  (`pulse_interval_min`/`pulse_duration_sec`), пин, enabled. При первом
  запуске сидятся дефолтами из Kconfig; новые ключи досидываются на старых
  устройствах без стирания NVS.
- `main/wifi_setup.*` — Wi-Fi station, автопереподключение, hostname =
  `CONFIG_LAMP_DEVICE_NAME`.
- `main/time_sync.*` — часовой пояс (POSIX TZ) + SNTP через `esp_netif_sntp`.
- `main/lamp.*` — GPIO и цикл применения режима: расписание — тик раз в
  15 с (поддерживает расписание через полночь), импульсы — тик раз в 1 с
  (15-секундный тик съест короткий импульс). Чистая логика — в `schedule.*`
  (`lamp_schedule_active`, `lamp_pulse_active`) и `hhmm.*` (разбор/формат
  "HH:MM"), они без зависимостей от ESP-IDF и покрыты host-тестами из `test/`.
- `main/ota_update.*` — OTA с GitHub Releases: раз в
  `CONFIG_LAMP_OTA_CHECK_INTERVAL_MIN` опрашивает `releases/latest`,
  сравнивает semver с версией прошивки и обновляется через esp_https_ota.
  Важно: буферы HTTP 2048 — подписанный URL после 302-редиректа GitHub
  длиной ~900+ байт не влезает в дефолтные 512.
- `main/web_server.*` — локальный веб-интерфейс (порт 80): исходники
  на Vue в `web/src/`, собираются в `web/dist/index.html.gz`
  (встраивается через EMBED_FILES) + REST
  `GET/POST /api/settings` (режим `mode`, времена в "HH:MM", параметры
  импульсов) и `GET /api/info`
  (имя/версия/чип — задел под сканер флота). Изменения сохраняются в NVS
  и применяются на лету через `lamp_apply_settings`.
- `main/Kconfig.projbuild` — дефолты: Wi-Fi, пин, времена, режим и параметры
  импульсов, TZ, имя устройства, OTA (репозиторий, интервал).
- `partitions.csv` — разметка флеша: 2 OTA-слота по ~1.9M, без factory.
- `docs/ota.md` — полная документация по OTA (схема, настройки, диагностика).
- `tools/scan.sh` — поиск ламп в локалке: обходит подсеть, дёргает
  `/api/info`, печатает URL (`http://IP/`) + JSON ответивших устройств.
- `.agents/skills/` — проектные скиллы: `ota`, `firmware-release`.
- `ideas/` — концепции будущего развития (веб-интерфейс, флот, MQTT);
  обнаружение устройств в LAN — готовая спека `ideas/mdns-discovery.md`.

## Соглашения

- **Kconfig = дефолты первого запуска, NVS = живые настройки.** Всё, что
  должно меняться без перепрошивки, читается/пишется через `settings_*`.
  Новые настройки добавляем и туда, и туда.
- Логирование — `ESP_LOGx` с тегом модуля, сообщения по-русски (прошивка
  личная, монитор читает владелец).
- Код комментируем по-русски, идентификаторы — по-английски.
- Минимальные изменения, без преждевременных абстракций; задел на будущее
  только там, где он уже есть (`lamp_apply_settings`, `settings_save`).
- Расписание храним в минутах от полуночи (`on_min`/`off_min`), строки HH:MM
  только на границах (Kconfig, HTTP API). Импульсы — `pulse_interval_min` в
  минутах и `pulse_duration_sec` в секундах, привязаны к полуночи: начало
  интервала кратно `pulse_interval_min` минутам от 00:00 (переживает
  перезагрузки, детерминировано). Режим (`mode`) переключается через
  веб-интерфейс, перепрошивка не нужна.

## Roadmap (кратко, детали в `ideas/future.md`)

1. **v1 (сейчас)**: расписание из NVS, Wi-Fi + SNTP, один пин, OTA с GitHub
   Releases (релиз = git-тег → CI → устройство само подхватывает).
2. **v2**: локальный веб-интерфейс на устройстве (сделан: `web_server.*`,
   REST + страничка, настройки сохраняются в NVS и применяются на лету),
   mDNS `ebbflow-lamp-N.local`.
3. **v3**: несколько устройств: device ID, обнаружение в LAN, возможно MQTT +
   внешний контроллер.
