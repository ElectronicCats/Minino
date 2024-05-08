set(modules "modules/keyboard/keyboard_module.c"
            "modules/menu_screens/menu_screens_modules.c"
            "modules/zigbee/zigbee_switch.c"
            "modules/zigbee/zigbee_screens_module.c")

set(drivers "drivers/oled_driver.c")

idf_component_register(
                    SRCS
                    "main.c"
                    "${modules}"
                    "${drivers}"
                    INCLUDE_DIRS
                    "."
                    "drivers/"
                    "modules/menu_screens"
                    "modules/keyboard/"
                    "modules/zigbee/")
