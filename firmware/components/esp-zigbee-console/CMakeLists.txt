include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_ZIGBEE_STACK _minino_opt_on)

if(_minino_opt_on)
    if (CONFIG_ZB_ZED)
        message(FATAL_ERROR "${COMPONENT_NAME} doesn't support CONFIG_ZB_ZED=y")
    endif()

    idf_component_register(SRC_DIRS "src" "src/zb_data"
                           INCLUDE_DIRS "include"
                           PRIV_REQUIRES esp-zigbee-lib console
                           LDFRAGMENTS linker.lf
                           WHOLE_ARCHIVE)
else()
    idf_component_register()
endif()
