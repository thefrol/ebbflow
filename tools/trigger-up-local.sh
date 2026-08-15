#!/bin/bash
# trigger-up-local.sh — ручной запуск OTA-обновления для ebbflow-lamp в локальной сети.
# Сканирует подсеть, находит лампы, дергает /api/update/check и, если есть новая
# версия, стартует скачивание через /api/update/start. После старта отслеживает
# статус до перезагрузки.
#
# Примеры:
#   tools/trigger-up-local.sh              # определить подсеть автоматически
#   tools/trigger-up-local.sh 192.168.1    # задать подсеть явно
#   tools/trigger-up-local.sh -y             # не спрашивать подтверждение
#   tools/trigger-up-local.sh -y 192.168.1

set -u

YES=0
SUBNET=""

while [ $# -gt 0 ]; do
    case "$1" in
        -y|--yes) YES=1; shift ;;
        -*) echo "неизвестный флаг: $1" >&2; exit 1 ;;
        *) SUBNET="$1"; shift ;;
    esac
done

if [ -z "$SUBNET" ]; then
    IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null || true)
    if [ -z "$IP" ]; then
        echo "не удалось определить локальный IP, задай подсеть явно: $0 192.168.1" >&2
        exit 1
    fi
    SUBNET="${IP%.*}"
fi

for cmd in curl python3; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "нужен $cmd, но не найден" >&2
        exit 1
    fi
done

PY_GET='import sys,json; d=json.load(sys.stdin); print(d.get(sys.argv[1], "?"))'
PY_VALIDATE='import sys,json; d=json.loads(sys.argv[1]); assert "chip" in d and "version" in d'

scan_file=$(mktemp)
trap 'rm -f "$scan_file"' EXIT

echo "сканирую $SUBNET.0/24 (порт 80, /api/info) ..."
for i in $(seq 1 254); do
    (
        resp=$(curl -s --max-time 1 "http://$SUBNET.$i/api/info" 2>/dev/null)
        if python3 -c "$PY_VALIDATE" "$resp" >/dev/null 2>&1; then
            echo "http://$SUBNET.$i/ $resp" >> "$scan_file"
        fi
    ) &
done
wait

found=$(wc -l < "$scan_file" | tr -d ' ')
if [ "$found" -eq 0 ]; then
    echo "лампы не найдены"
    exit 0
fi

echo ""
echo "найдено устройств: $found"
while read -r url info; do
    ip=${url#http://}
    ip=${ip%/}
    name=$(echo "$info" | python3 -c "$PY_GET" name)
    version=$(echo "$info" | python3 -c "$PY_GET" version)
    chip=$(echo "$info" | python3 -c "$PY_GET" chip)
    echo "  - $ip  $name  $version  $chip"
done < "$scan_file"

if [ "$YES" -ne 1 ]; then
    echo ""
    read -r -p "запустить обновление для всех найденных устройств? [y/N] " ans
    case "$ans" in
        y|Y|yes|YES) ;;
        *) echo "отмена"; exit 0 ;;
    esac
fi

echo ""
while read -r url info; do
    ip=${url#http://}
    ip=${ip%/}
    name=$(echo "$info" | python3 -c "$PY_GET" name)
    version=$(echo "$info" | python3 -c "$PY_GET" version)
    chip=$(echo "$info" | python3 -c "$PY_GET" chip)

    echo "=== $ip ($name, $version, $chip) ==="

    check_resp=$(curl -sS --max-time 30 -X POST "http://$ip/api/update/check" 2>/dev/null || echo '{}')
    state=$(echo "$check_resp" | python3 -c "$PY_GET" state 2>/dev/null || echo "unreachable")
    available=$(echo "$check_resp" | python3 -c "$PY_GET" available_version 2>/dev/null || echo "?")
    err=$(echo "$check_resp" | python3 -c "$PY_GET" error 2>/dev/null || echo "?")
    echo "  check -> state=$state available=$available error=$err"

    if [ "$state" = "up_to_date" ] || [ "$state" = "idle" ]; then
        echo "  -> уже актуально, пропускаем"
        continue
    fi

    if [ "$state" != "update_available" ]; then
        echo "  -> нет доступного обновления, пропускаем"
        continue
    fi

    start_full=$(curl -sS -i --max-time 10 -X POST "http://$ip/api/update/start" 2>/dev/null || true)
    start_code=$(echo "$start_full" | head -n 1 | awk '{print $2}')
    echo "  start -> HTTP $start_code"

    if [ "$start_code" = "404" ]; then
        echo "  -> внимание: POST /api/update/start не поддерживается этой прошивкой (вероятно, < v0.11.0)."
        echo "     для запуска OTA перезагрузи устройство физически — при старте оно само скачает обновление."
        continue
    fi

    if [ "$start_code" != "200" ]; then
        echo "  -> запуск обновления не удался:"
        echo "$start_full" | sed 's/^/     /'
        continue
    fi

    echo "  -> обновление запущено, жду ..."
    for i in $(seq 1 120); do
        status_resp=$(curl -sS --max-time 5 "http://$ip/api/update/status" 2>/dev/null || echo '{}')
        st=$(echo "$status_resp" | python3 -c "$PY_GET" state 2>/dev/null || echo "unreachable")
        er=$(echo "$status_resp" | python3 -c "$PY_GET" error 2>/dev/null || echo "")
        echo "    $(date '+%H:%M:%S') state=$st${er:+ error=$er}"
        case "$st" in
            idle|up_to_date|error|reboot_pending|unreachable) break ;;
        esac
        sleep 5
    done

    if [ "$st" = "reboot_pending" ] || [ "$st" = "unreachable" ]; then
        echo "  -> устройство перезагружается, жду 25 с ..."
        sleep 25
        new_info=$(curl -sS --max-time 5 "http://$ip/api/info" 2>/dev/null || echo '{}')
        new_version=$(echo "$new_info" | python3 -c "$PY_GET" version 2>/dev/null || echo "?")
        echo "    после перезагрузки: version=$new_version"
    fi

echo ""
done < "$scan_file"

echo "готово"
