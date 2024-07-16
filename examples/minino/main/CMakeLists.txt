set(modules "modules/led_events/led_events.c"
            "modules/keyboard/keyboard_module.c"
            "modules/menu_screens/menu_screens_modules.c"
            "modules/oled_screen/oled_screen.c"
            "modules/settings/settings_module.c"
            "modules/wifi/wifi_module.c"
            "modules/wifi/wifi_screens_module.c"
            "modules/zigbee/zigbee_module.c"
            "modules/zigbee/zigbee_screens_module.c"
            "modules/ble/ble_module.c"
            "modules/ble/ble_screens_module.c"
            "modules/open_thread/open_thread_module.c"
            "modules/open_thread/open_thread_screens_module.c"
            "modules/gps/gps_module.c"
            "modules/wardriving/wardriving_module.c")

set(drivers "drivers/oled_driver/oled_driver.c"
            "drivers/oled_driver/oled_driver_i2c.c"
            "drivers/oled_driver/oled_driver_spi.c")

idf_component_register(
                    SRCS
                    "main.c"
                    "${modules}"
                    "${drivers}"
                    INCLUDE_DIRS
                    "."
                    "modules/keyboard/"
                    "modules/menu_screens/"
                    "drivers/oled_driver/"
                    "modules/oled_screen/"
                    "modules/settings/"
                    "modules/wifi/"
                    "modules/ble/"
                    "modules/zigbee/"
                    "modules/led_events"
                    "modules/open_thread"
                    "modules/gps"
                    "modules/wardriving")

# # Append all directories to INCLUDE_DIRS

# file(GLOB_RECURSE ALL_INCLUDE_PATHS "./*")
# set(INCLUDE_DIRS "")

# foreach(path ${ALL_INCLUDE_PATHS})
#     get_filename_component(parentDir ${path} DIRECTORY)

#     if(IS_DIRECTORY ${parentDir})
#         list(APPEND INCLUDE_DIRS ${parentDir})
#     endif()
# endforeach()

# list(REMOVE_DUPLICATES INCLUDE_DIRS)

# # Append all source files to SRCS

# file(GLOB_RECURSE ALL_SOURCE_FILES "./*")
# set(SRCS "")

# foreach(file ${ALL_SOURCE_FILES})
#     get_filename_component(extension ${file} EXT)
#     if(NOT extension STREQUAL "")
#         list(APPEND SRCS ${file})
#     endif()
# endforeach()

# # Register the component

# idf_component_register(
#                     SRCS
#                     ${SRCS}
#                     INCLUDE_DIRS
#                     ${INCLUDE_DIRS})
