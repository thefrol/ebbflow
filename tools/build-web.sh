#!/usr/bin/env bash
# Сборка frontend в web/ и подготовка сжатого index.html.gz для встраивания в прошивку.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WEB_DIR="${REPO_ROOT}/web"

cd "${WEB_DIR}"

if ! command -v node >/dev/null 2>&1; then
    echo "ERROR: для сборки frontend нужен Node.js (npm run build в ${WEB_DIR})"
    exit 1
fi

if [ ! -d "node_modules" ]; then
    echo "web: установка зависимостей..."
    npm ci
fi

echo "web: сборка..."
npm run build
