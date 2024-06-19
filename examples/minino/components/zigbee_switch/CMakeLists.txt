idf_component_register(SRCS "zigbee_switch.c"
                    INCLUDE_DIRS "include"
                    REQUIRES espressif__esp-zigbee-lib esp_driver_uart)
