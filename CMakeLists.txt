# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NEVER EDIT THE NEXT LINES for Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.0.0)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# Custom boards directory
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards/include/boards)
set(PICO_BOARD custom_catchcam CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/freertos)
include(FreeRTOS_Kernel_import.cmake)

project(catchcam C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(catchcam
    src/audio/audio.c
    src/cams/cams.c
    src/gnss/minmea/minmea.c
    src/gnss/gnss.c
    src/leds/leds.c
    src/main.c
)

target_include_directories(catchcam PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

# enable usb output, disable uart output
pico_enable_stdio_usb(catchcam 1)
pico_enable_stdio_uart(catchcam 0)

# pull in common dependencies
target_link_libraries(catchcam PRIVATE
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_irq
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap1
)

# by default the header is generated into the build dir
pico_generate_pio_header(catchcam ${CMAKE_CURRENT_LIST_DIR}/src/audio/i2s_lsbj.pio)

# create map/bin/hex file etc.
pico_add_extra_outputs(catchcam)

# TODO: add url via pico_set_program_url
# pico_set_program_url()
