# Host-тесты чистой логики лампы (цель linux, запуск без железа):
#   idf.py set-target linux && idf.py build && ./build/lamp_host_tests.elf
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(lamp_host_tests)
