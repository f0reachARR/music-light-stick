cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr)
project(penlight LANGUAGES C)

target_sources(app PRIVATE
    src/main.c
    src/led_controller.c
    src/button.c
    src/color_utils.c
    src/effect_engine.c
    src/preset_manager.c
    src/preview_manager.c
    src/power_manager.c
    src/ble_service.c
)

target_include_directories(app PRIVATE src)
