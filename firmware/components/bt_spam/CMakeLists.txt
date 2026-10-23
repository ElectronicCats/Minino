include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_BT_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "bt_spam.c"
                         "bt_spam_gap_sync.c"
                         PRIV_REQUIRES bt nvs_flash bt_gattc gap_dispatcher radio_lease
                         INCLUDE_DIRS ".")
else()
    idf_component_register()
endif()