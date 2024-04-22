set(modules "modules/keyboard_module.c" "modules/menu_screen_modules.c")

idf_component_register(SRCS "main.c" "esp_zb_switch.c" "${modules}"
                    INCLUDE_DIRS ".")
