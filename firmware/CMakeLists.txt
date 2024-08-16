# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components/oled_driver)
set(EXTRA_COMPONENT_DIRS components/wifi_sniffer)
set(EXTRA_COMPONENT_DIRS components/ble_hid)
set(EXTRA_COMPONENT_DIRS components/buzzer)
set(EXTRA_COMPONENT_DIRS components/files_ops)
set(EXTRA_COMPONENT_DIRS components/flash_fs)
set(EXTRA_COMPONENT_DIRS components/iperf)
set(EXTRA_COMPONENT_DIRS components/leds)
set(EXTRA_COMPONENT_DIRS components/sd_card)
set(EXTRA_COMPONENT_DIRS components/preferences)
set(EXTRA_COMPONENT_DIRS components/ieee802154)
set(EXTRA_COMPONENT_DIRS components/openthread)
set(EXTRA_COMPONENT_DIRS components/thread_broadcast)
set(EXTRA_COMPONENT_DIRS components/radio_selector)
set(EXTRA_COMPONENT_DIRS components/wifi_captive)
set(EXTRA_COMPONENT_DIRS components/wifi_app)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_VERSION_MAJOR(0))
set(PROJECT_VERSION_MINOR(1))
set(PROJECT_VERSION_PATCH(0))
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
project(minino)
