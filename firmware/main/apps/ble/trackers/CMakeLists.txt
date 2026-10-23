include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(BT_ENABLED _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "trackers_module.c"
                                    "trackers_screens.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES trackers_scanner gap_dispatcher radio_lease)
endif()
