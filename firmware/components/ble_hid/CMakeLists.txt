include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_BT_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "ble_hidd_main.c"
                            "esp_hidd_prf_api.c"
                            "hid_dev.c"
                            "hid_device_le_prf.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt nvs_flash esp_driver_gpio gap_dispatcher radio_lease)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
else()
    idf_component_register()
endif()