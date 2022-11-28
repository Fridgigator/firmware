cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK}/external/pico_sdk_import.cmake)

project(weather_sensor C CXX ASM)
set(CMAKE_C STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(weather_sensor main.cpp)

pico_enable_stdio_usb(weather_sensor 1)
#pico_enable_stdio_uart(weather_sensor 0)

pico_add_extra_outputs(weather_sensor)
add_subdirectory(dht)
target_include_directories(weather_sensor PRIVATE ${CMAKE_CURRENT_LIST_DIR} )


target_link_libraries(weather_sensor pico_stdlib dht hardware_adc)