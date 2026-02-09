cmake_minimum_required(VERSION 3.20)

add_compile_options(
        -Wall
        -Wextra
        -Wfloat-equal
        -Wunreachable-code
        -Wno-unused-function
        -Wno-maybe-uninitialized
        -Wno-ignored-qualifiers
        -Wno-stringop-overflow          # rp2040_usb.c:61:3
        -Wno-array-bounds               # rp2040_usb.c:61:3
        )

include(pico_sdk_import.cmake)

project(profitec_scale C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)


# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(extern/pico-scale)
add_subdirectory(extern/displaylib)

add_executable(main
	main.cpp
)

target_link_libraries(main pico_stdlib pico_stdio hardware_i2c hardware_spi hardware_pio pico-scale pico_displaylib)

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)
pico_add_extra_outputs(main)
