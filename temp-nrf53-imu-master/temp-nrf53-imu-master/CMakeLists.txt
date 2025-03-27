cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(solo-dongle)

file(GLOB SRC_FILES src/*.c)

target_sources(app PRIVATE 
    ${SRC_FILES}
)

add_subdirectory(src/imu)
add_definitions(-DICM42670S)