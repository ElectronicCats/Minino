set(modules "modules/keyboard/keyboard_module.c"
            "modules/menu_screens/menu_screens_modules.c"
            "modules/oled_screen/oled_screen.c"
            "modules/zigbee/zigbee_switch.c"
            "modules/zigbee/zigbee_screens_module.c")

idf_component_register(
                    SRCS
                    "main.c"
                    "${modules}"
                    INCLUDE_DIRS
                    "."
                    "modules/menu_screens"
                    "modules/oled_screen"
                    "modules/keyboard/"
                    "modules/zigbee/")
