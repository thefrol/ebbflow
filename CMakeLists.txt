cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Версия прошивки. Релизные сборки в CI переопределяют её тегом
# (idf.py build -DPROJECT_VER=x.y.z); OTA сравнивает её с тегом релиза.
project(ebbflow-lamp VERSION 0.11.3)

# Сборка frontend (Vue 3 + Vite) и сжатие index.html для встраивания в прошивку.
# Запускается как отдельная цель, от которой зависит app.
set(WEB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/web")
set(WEB_INDEX_GZ "${WEB_DIR}/dist/index.html.gz")
file(GLOB_RECURSE WEB_SRC_FILES
    "${WEB_DIR}/src/*.ts"
    "${WEB_DIR}/src/*.vue"
    "${WEB_DIR}/src/*.css"
    "${WEB_DIR}/index.html"
    "${WEB_DIR}/package.json"
    "${WEB_DIR}/package-lock.json"
    "${WEB_DIR}/vite.config.ts"
    "${WEB_DIR}/tsconfig.json"
    "${WEB_DIR}/tsconfig.node.json"
    "${WEB_DIR}/scripts/*.js"
)

add_custom_command(
    OUTPUT ${WEB_INDEX_GZ}
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tools/build-web.sh
    DEPENDS ${WEB_SRC_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building web frontend"
)
add_custom_target(web_frontend ALL DEPENDS ${WEB_INDEX_GZ})
add_dependencies(app web_frontend)
