idf_component_register(SRCS "zb_cli.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash espressif__esp-zigbee-lib esp-zigbee-console console)