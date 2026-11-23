cmake_minimum_required(VERSION 3.13)

project(catchcam_test C)

set(CMAKE_C_STANDARD 11)

set(SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

add_executable(test_drive
    test_drive.c
    drive.c
    unity/unity.c
    ${SRC}/alert/alert.c
    ${SRC}/cams/cams.c
    ${SRC}/gnss/gnss_nmea.c
    ${SRC}/gnss/minmea/minmea.c
)

target_include_directories(test_drive PRIVATE ${SRC} shim unity)
target_compile_options(test_drive PRIVATE -Wall -Wextra -fsanitize=address,undefined)
target_link_options(test_drive PRIVATE -fsanitize=address,undefined)
target_link_libraries(test_drive PRIVATE m)

enable_testing()
add_test(NAME drive COMMAND test_drive)
