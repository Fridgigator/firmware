cmake_minimum_required(VERSION 3.13)

link_directories(${CMAKE_SOURCE_DIR}/rust_module/target/thumbv6m-none-eabi/release nec_receive_library)

include(pico_sdk_import.cmake)

project(firmare C CXX ASM)

find_package(Corrosion REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_subdirectory(nec_receive_library)
add_executable(firmware main.c lcd-2004/lcd_2004_i2c.c lcd-2004/lcd_2004_i2c.h)
pico_enable_stdio_uart(firmware 1)
pico_add_extra_outputs(firmware)
target_link_libraries(firmware LINK_PUBLIC hardware_adc pico_stdlib start_rust pico_stdlib hardware_i2c hardware_pio nec_receive_library)

corrosion_import_crate(MANIFEST_PATH start_rust/Cargo.toml)
