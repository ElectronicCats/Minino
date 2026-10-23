include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_ZIGBEE_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "zb_cli.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash espressif__esp-zigbee-lib esp-zigbee-console console radio_selector)
else()
    idf_component_register()
endif()