# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS 
    "components"
    "main/drivers/oled_driver"
)


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
project(minino)

# Automatically generate unified merged .bin and .hex on every standard build
add_custom_target(minino_merged ALL
    COMMAND ${PYTHON} -m esptool --chip ${IDF_TARGET} merge_bin
        -o "${CMAKE_BINARY_DIR}/minino-merged.bin"
        @flash_args
    COMMAND ${CMAKE_OBJCOPY} -I binary -O ihex --change-addresses 0x0
        "${CMAKE_BINARY_DIR}/minino-merged.bin"
        "${CMAKE_BINARY_DIR}/minino-merged.hex"
    COMMAND ${CMAKE_COMMAND} -E echo "Generated unified binaries: minino-merged.bin and minino-merged.hex"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    DEPENDS app bootloader partition_table_bin
    COMMENT "Generating unified minino-merged.bin and minino-merged.hex"
)

