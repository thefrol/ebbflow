# Бандлер фронтенда: переход на vite-plugin-singlefile

## Проблема

Сейчас фронтенд собирается Vite, но по умолчанию Vite выдаёт `index.html`
плюс отдельные ассеты (`assets/index.js`, `assets/index.css`). Чтобы встроить
всё в прошивку одним файлом, используется костыль
`web/scripts/inline-assets.js`: он после `vite build` парсит HTML, читает
CSS/JS, вставляет их в `<style>` и `<script type="module">` вручную,
экранирует `</script>` и удаляет папку `assets`.

Это ломко и не нужно: есть штатный плагин `vite-plugin-singlefile`, который
делает inline внутри пайплайна Vite.

## План

1. Установить `vite-plugin-singlefile` в `web/`.
2. Заменить `plugins: [vue()]` на `plugins: [vue(), vitePluginSinglefile()]`
   в `web/vite.config.ts`.
3. Убрать `rollupOptions.output.entryFileNames/chunkFileNames/assetFileNames`,
   `cssCodeSplit` и прочие настройки, которые были нужны только для
   `inline-assets.js` (хэши больше не важны, плагин сам всё свернёт).
4. Удалить `web/scripts/inline-assets.js` и вызов `node scripts/inline-assets.js`
   из `tools/build-web.sh` (или вообще заменить `build-web.sh` на
   `npm run build` внутри `web/`).
5. Убедиться, что `web/dist/index.html` остаётся одним самодостаточным
   gzip-файлом, который `EMBED_FILES` в прошивку встраивает как раньше.
6. Собрать под обе цели (`esp32c3`, `esp32c6`) и проверить на устройстве:
   `GET /` отдаёт `text/html` + gzip, веб-интерфейс открывается, кнопки
   работают, `/api/settings` сохраняет настройки.

## Критерий готовности

- `web/scripts/inline-assets.js` удалён.
- `tools/build-web.sh` не вызывает пост-скрипт, только `npm run build`.
- `web/vite.config.ts` использует `vite-plugin-singlefile`.
- Релизная сборка CI под обе цели проходит без ошибок.
- Веб на устройстве открывается и работает после обновления OTA.
