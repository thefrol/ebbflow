cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Версия прошивки. Релизные сборки в CI переопределяют её тегом
# (idf.py build -DPROJECT_VER=x.y.z); OTA сравнивает её с тегом релиза.
project(ebbflow-lamp VERSION 0.3.0)
