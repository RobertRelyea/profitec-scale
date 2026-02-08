cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(profitec_scale C CXX ASM)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_subdirectory(extern/pico-scale)

add_executable(main
	main.c
)

target_link_libraries(main pico_stdlib pico_stdio hardware_pio pico-scale)
pico_add_extra_outputs(main)
