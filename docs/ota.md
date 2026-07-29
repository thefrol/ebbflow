# OTA — обновление прошивки по воздуху

Устройство само следит за релизами на GitHub и обновляется по HTTPS.
Своя инфраструктура не нужна: реестр артефактов — GitHub Releases,
сборка — GitHub Actions, доставка — `esp_https_ota` в прошивке.

## Общая схема

```
разработчик                GitHub                       устройство
───────────                ──────                       ──────────
git tag v0.5.0  ──►  Actions: сборка под esp32c3/c6
git push --tags      в контейнере espressif/idf:v6.0.2
                     релиз v0.5.0 + ассеты:
                       ebbflow-lamp-esp32c3.bin
                       ebbflow-lamp-esp32c6.bin
                                                     раз в час:
                                                     GET api.github.com/repos/
                                                       thefrol/ebbflow/releases/latest
                                                     tag новее своей версии? ──►
                                                     esp_https_ota качает .bin
                                                     в свободный слот, ребут
```

## Устройство: `main/ota_update.c`

Фоновая задача FreeRTOS (запускается из `app_main` после Wi-Fi + SNTP):

1. `GET https://api.github.com/repos/<CONFIG_LAMP_OTA_REPO>/releases/latest`
   — заголовок `User-Agent` обязателен (иначе GitHub отвечает 403).
2. Сравнение версий: `tag_name` релиза против `esp_app_get_description()->version`
   (semver, префикс `v` отбрасывается; прошивка с не-semver версией —
   например локальная сборка — считается старой и обновляется на релиз).
3. Если релиз новее — ищется ассет `ebbflow-lamp-<CONFIG_IDF_TARGET>.bin`,
   его `browser_download_url` скармливается `esp_https_ota`, затем `esp_restart()`.

### Критические настройки (не ломать!)

- **Буферы HTTP 2048.** GitHub отвечает на `browser_download_url` редиректом
  302 на `release-assets.githubusercontent.com` с подписанным URL длиной
  ~900+ байт. Дефолтный буфер `esp_http_client` — 512 байт, URL не влезает
  и запрос падает с `Out of buffer`. Поэтому в `ota_from_url()`:
  `buffer_size = 2048, buffer_size_tx = 2048`.
- **TLS — только cert bundle** (`esp_crt_bundle_attach`, включён по умолчанию
  в IDF v6). Он покрывает и github.com (Sectigo/USERTrust ECC), и CDN
  ассетов (Let's Encrypt/ISRG). Свой сертификат не вшивать: GitHub меняет
  CA, пиннинг гарантированно сломается.
- **Rate limit**: анонимный GitHub API — 60 запросов/час на IP (все
  устройства за одним NAT делят лимит). Поэтому интервал проверки
  `CONFIG_LAMP_OTA_CHECK_INTERVAL_MIN` по умолчанию 60 минут.

## Версионирование

- Версия прошивки = `project(ebbflow-lamp VERSION x.y.z)` в корневом
  `CMakeLists.txt`.
- CI переопределяет её тегом: `idf.py build -DPROJECT_VER="${GITHUB_REF_NAME#v}"`.
- Правило: **тег = версия в CMakeLists = версия релиза**. При выпуске
  релиза сначала поднимаем VERSION, коммитим, потом тег.

## Разметка флеша: `partitions.csv`

Своя разметка вместо штатной `partitions_two_ota` (в IDF v6 она держит
ещё и `factory` 1M, и на 4 МБ флеша слоты получаются по 1M — прошивка
с TLS/OTA не влезала даже с -Os на esp32c6):

```
nvs      0x9000   0x6000
otadata  0xf000   0x2000
phy_init 0x11000  0x1000
ota_0    0x20000  0x1E0000   (~1.9 МБ)
ota_1    0x200000 0x1E0000   (~1.9 МБ)
```

Factory-раздела нет: кабельная прошивка (`idf.py flash`) идёт в `ota_0`,
OTA пишет в противоположный слот и переключает загрузку на него.
Менять разметку на прошитом устройстве = стереть флеш целиком
(`esptool erase-flash`) и прошить заново.

## Откат при неудачной загрузке

`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` (в `sdkconfig.defaults`).
Новая прошивка грузится в состоянии «pending verification» и обязана
подтвердить себя вызовом `esp_ota_mark_app_valid_cancel_rollback()` —
у нас это в `app_main` после подключения Wi-Fi и синхронизации времени.
Если прошивка не загрузилась или не дошла до самотеста — загрузчик
возвращает предыдущий слот.

Не включено (осознанно, для личной лампы избыточно): Secure Boot V2 и
anti-rollback через eFuse — необратимо и усложняет производство.

## CI: `.github/workflows/release.yml`

- Триггер: пуш тега `v*`.
- Матрица `esp32c3`/`esp32c6`, контейнер `espressif/idf:v6.0.2`.
  GitHub Actions перебивает entrypoint контейнера, поэтому первый шаг —
  `. "$IDF_PATH/export.sh"`, иначе `idf.py: not found`.
- Wi-Fi креды — из GitHub Secrets (`WIFI_SSID`, `WIFI_PASSWORD`) в
  `sdkconfig.defaults.local` на время сборки. Обязательно: креды зашиваются
  в бинарь, без этого шага устройство после OTA потеряло бы сеть.
- Релиз публикует `softprops/action-gh-release`.

## Как проверить OTA на столе

1. Прошить устройство кабелем локальной сборкой с версией НИЖЕ будущего
   релиза и `CONFIG_LAMP_OTA_CHECK_INTERVAL_MIN=1` в `sdkconfig`.
2. Поднять VERSION в CMakeLists, закоммитить, запушить тег.
3. Дождаться зелёного CI и релиза: `gh run watch`, `gh release view`.
4. Устройство в течение минуты само скачает и применит обновление —
   в мониторе: `есть новая версия` → `скачиваю обновление` →
   `Writing to <ota_N>` → ребут → `vX.Y.Z: старт`.

Нюанс: `idf.py monitor` требует TTY; из скриптов порт читается напрямую
(pyserial, 115200), ресет — импульсом RTS при отпущенном DTR.

## Диагностика

| Симптом в логе | Причина | Лечение |
|---|---|---|
| `HTTP 403` от api.github.com | нет User-Agent или исчерпан rate limit | проверить заголовок; подождать час |
| `HTTP 404` от releases/latest | релизов нет / репо приватный | создать релиз; приватные репо не поддержаны (нужен токен, не реализовано) |
| `Out of buffer`, `esp_http_client_open failed` | буферы < 2048, не влез редиректный URL | см. «Критические настройки» |
| TLS handshake failed | старый прошивочный cert bundle / пиннинг | использовать `esp_crt_bundle_attach` |
| `Running firmware is factory` при самотесте | прошивка кабелем в factory-раздел | безвредно; в нашей разметке factory нет |
| `тег релиза не похож на версию` | тег не вида `vX.Y.Z` | переименовать тег |
| устройство обновилось и молчит | OTA-образ без Wi-Fi кредов | Secrets в CI; откатится сам по rollback |

## Дальнейшее чтение

- Большое исследование со всеми источниками (лимиты GitHub, альтернативы,
  чужие проекты): отчёт DAI — https://dai.devdima.ru/reports/rpt-1785349714896256224
- `ideas/future.md` — что отложено (ETag, сверка sha256, ручной триггер
  из веб-интерфейса, secure boot).
