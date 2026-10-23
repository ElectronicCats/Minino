include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_ZIGBEE_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "zigbee_switch.c"
                    INCLUDE_DIRS "include"
                    REQUIRES espressif__esp-zigbee-lib esp_driver_uart radio_selector
                    WHOLE_ARCHIVE)
else()
    idf_component_register()
endif()