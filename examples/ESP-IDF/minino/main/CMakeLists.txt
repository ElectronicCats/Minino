set(modules "modules/led_events/led_events.c"
            "modules/keyboard/keyboard_module.c"
            "modules/menu_screens/menu_screens_modules.c"
            "modules/oled_screen/oled_screen.c"
            "modules/wifi/wifi_module.c"
            "modules/wifi/wifi_screens_module.c"
            "modules/zigbee/zigbee_module.c"
            "modules/zigbee/zigbee_screens_module.c"
            "modules/ble/ble_module.c"
            "modules/ble/ble_screens_module.c"
            "modules/open_thread/open_thread_module.c"
            "modules/open_thread/open_thread_screens_module.c")

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
                    "modules/wifi/"
                    "modules/ble/"
                    "modules/zigbee/"
                    "modules/led_events"
                    "modules/open_thread")
