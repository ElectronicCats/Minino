include("${CMAKE_CURRENT_LIST_DIR}/../../../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(BT_ENABLED _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "gattcmd_module.c"
                                    "gattcmd_cmd.c"
                                    "services/gattcmd_enum.c"
                                    "services/gattcmd_scan.c"
                                    "services/gattcmd_recon.c"
                                    "services/gattcmd_write.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES bt_gattc gap_dispatcher radio_lease)
endif()
