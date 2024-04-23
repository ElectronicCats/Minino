set(modules "modules/keyboard/keyboard.c"
            "modules/menu_screens_modules.c")

idf_component_register(SRCS "main.c" "esp_zb_switch.c" "${modules}"
                    INCLUDE_DIRS "." "modules/" "modules/keyboard/")
