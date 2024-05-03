set(modules "modules/keyboard/keyboard.c"
            "modules/menu_screens/menu_screens_modules.c"
            "modules/zigbee/zigbee_light.c"
            "modules/zigbee/zigbee_switch.c"
            "modules/zigbee/zigbee_screens.c")

idf_component_register(SRCS "main.c" "${modules}"
                    INCLUDE_DIRS
                    "."
                    "modules/menu_screens"
                    "modules/keyboard/"
                    "modules/zigbee/")
