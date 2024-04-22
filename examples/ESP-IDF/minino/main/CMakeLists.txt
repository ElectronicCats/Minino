set(modules "modules/keyboard_module.c")

idf_component_register(SRCS "main.c" "display.c" "esp_zb_switch.c" "${modules}"
                    INCLUDE_DIRS "." "include")
