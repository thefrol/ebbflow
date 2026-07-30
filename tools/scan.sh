#!/bin/bash
# Поиск ebbflow-ламп в локальной сети: обходит подсеть и дёргает /api/info
# (эндпоинт веб-интерфейса, появился в v0.5.0). Отвечают только наши устройства.
#
# Использование:
#   tools/scan.sh                 # подсеть определяется по IP этого компьютера
#   tools/scan.sh 192.168.1       # или задать подсеть явно (первые 3 октета)

set -u

if [ $# -ge 1 ]; then
    SUBNET="$1"
else
    IP=$(ipconfig getifaddr en0 2>/dev/null || ipconfig getifaddr en1 2>/dev/null)
    if [ -z "$IP" ]; then
        echo "не удалось определить локальный IP, задай подсеть явно: $0 192.168.1" >&2
        exit 1
    fi
    SUBNET="${IP%.*}"
fi

echo "сканирую $SUBNET.0/24 (порт 80, /api/info)..."

for i in $(seq 1 254); do
    (
        resp=$(curl -s --max-time 1 "http://$SUBNET.$i/api/info" 2>/dev/null)
        # наш JSON всегда содержит version/chip — по ним и фильтруем
        # (порядок полей в ответе не гарантирован, проверяем оба)
        case "$resp" in
            *'"chip"'*'"version"'*|*'"version"'*'"chip"'*) echo "http://$SUBNET.$i/  $resp" ;;
        esac
    ) &
done
wait

echo "готово"
