cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(music_light_stick LANGUAGES C CXX)

target_sources(app PRIVATE src/main.cpp)
target_include_directories(app PUBLIC olaf)
