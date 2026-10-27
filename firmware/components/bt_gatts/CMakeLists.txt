include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_BT_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "bt_gatts.c"
                    PRIV_REQUIRES bt gap_dispatcher
                    INCLUDE_DIRS ".")
else()
    idf_component_register()
endif()