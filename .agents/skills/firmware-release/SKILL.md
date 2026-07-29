---
name: firmware-release
description: Выпуск релиза прошивки ebbflow-lamp — поднять версию, запушить тег, дождаться CI и при необходимости проверить OTA-обновление на устройстве. Использовать всегда, когда нужно опубликовать новую прошивку для устройств.
---

# Выпуск релиза прошивки

Релиз = git-тег `v*`. CI (`.github/workflows/release.yml`) собирает прошивку
под esp32c3 и esp32c6 и публикует GitHub Release с бинарниками; устройства
подхватывают его сами в течение часа.

## Чеклист

1. **Проверить, что код собирается под обе цели локально**
   (`idf.py set-target esp32c3 && idf.py build`, затем esp32c6) — CI
   падать не должен.
2. **Поднять версию** в корневом `CMakeLists.txt`:
   `project(ebbflow-lamp VERSION X.Y.Z)`. Тег будет `vX.Y.Z` — они обязаны
   совпадать, устройство сравнивает свою версию с тегом релиза.
3. **Закоммитить и запушить main.** ВАЖНО: тег ставится на коммит, из
   которого соберётся релиз — сначала пуш main, потом тег.
4. **Тег**: `git tag vX.Y.Z && git push origin vX.Y.Z`.
5. **Дождаться CI** (~4–6 мин):
   `gh run list --repo thefrol/ebbflow --limit 1` →
   `gh run watch <id> --repo thefrol/ebbflow --exit-status`.
6. **Проверить релиз**: `gh release view vX.Y.Z --repo thefrol/ebbflow` —
   должны быть ассеты `ebbflow-lamp-esp32c3.bin` и `-esp32c6.bin`.

## Переиздание релиза (тег уже запушен, но CI упал или релиз кривой)

```bash
git tag -d vX.Y.Z && git push origin :refs/tags/vX.Y.Z
# исправить, закоммитить, запушить main
git tag vX.Y.Z && git push origin vX.Y.Z
```

## Типичные падения CI

- `idf.py: not found` — забыт `. "$IDF_PATH/export.sh"` в шаге (контейнер
  espressif/idf, entrypoint перебивается Actions).
- `All app partitions are too small` — бинарь не влез в слот 0x1E0000:
  смотреть размер, возможно вернули -Og вместо -Os
  (`CONFIG_COMPILER_OPTIMIZATION_SIZE` в `sdkconfig.defaults`).
- устройство после OTA теряет сеть — протухли/не заданы секреты
  `WIFI_SSID`/`WIFI_PASSWORD` в настройках репозитория.

## После релиза

Устройства с дефолтным интервалом обновятся в течение часа сами. Для
немедленной проверки — перезагрузить устройство (проверка идёт при
старте) и смотреть лог с тегом `ota`. Подробности — скилл `ota`
и `docs/ota.md`.
