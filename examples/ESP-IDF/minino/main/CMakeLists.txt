set(modules "modules/keyboard/keyboard_module.c"
            "modules/menu_screens/menu_screens_modules.c"
            "modules/oled_screen/oled_screen.c"
            "modules/zigbee/zigbee_switch.c"
            "modules/zigbee/zigbee_screens_module.c")

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
                    "drivers/oled_driver/"
                    "modules/menu_screens/"
                    "modules/oled_screen/"
                    "modules/keyboard/"
                    "modules/zigbee/")
