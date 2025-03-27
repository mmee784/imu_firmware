add_definitions(-DICM42670S)

set(SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/imu.c
)

file(GLOB SRC_FILES imu.c)
file(GLOB IMU_DRIVER_FILES drivers/imu/*.c)
file(GLOB EMBUTILS_FILES drivers/Invn/EmbUtils/*.c)

target_sources(app PRIVATE 
    ${SRC_FILES}
    ${IMU_DRIVER_FILES}
    ${EMBUTILS_FILES}
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/imu
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/Invn
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers/Invn/EmbUtils
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

target_link_libraries(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/libInvnAlgoAML_42670S.a)
