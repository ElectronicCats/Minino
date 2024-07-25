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
            "modules/ota/ota_module.c"
            "modules/ota/ota_module_screens.c"
            "modules/cat_dos/catdos_module.c"
            "modules/cat_dos/cat_console.c"
            "modules/cat_dos/cmd_catdos.c"
            "modules/menu_screens/configuration.c"
            "modules/gps/gps_module.c"
            "modules/gps/wardriving/wardriving_module.c"
            "modules/gps/wardriving/wardriving_screens_module.c"
            "modules/web_file_browser/http_server/web_file_browser.c"
            "modules/web_file_browser/web_file_browser_module.c"
            "modules/web_file_browser/web_file_browser_screens.c")

set(drivers "drivers/oled_driver/oled_driver.c"
            "drivers/oled_driver/oled_driver_i2c.c"
            "drivers/oled_driver/oled_driver_spi.c")

idf_component_register(
                    SRCS
                    "main.c"
                    "${modules}"
                    "${drivers}"
                    EMBED_FILES modules/web_file_browser/http_server/header.html modules/web_file_browser/http_server/favicon_b.ico
                    modules/web_file_browser/http_server/style.css
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
                    "modules/web_file_browser/http_server"
                    "modules/web_file_browser"
                    "modules/ota"
                    "modules/cat_dos/"
                    "modules/gps"
                    "modules/gps/wardriving")

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
