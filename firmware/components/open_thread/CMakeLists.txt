include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_OPENTHREAD_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "open_thread.c"
                    INCLUDE_DIRS "include"
                    REQUIRES openthread
                    PRIV_REQUIRES esp_event esp_netif nvs_flash)
else()
    idf_component_register()
endif()