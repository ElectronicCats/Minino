include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_ZIGBEE_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "zigbee_light.c"
                    INCLUDE_DIRS "include"
                    REQUIRES espressif__esp-zigbee-lib radio_selector leds)
else()
    idf_component_register()
endif()