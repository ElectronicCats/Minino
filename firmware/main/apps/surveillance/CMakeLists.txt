include("${MININO_KCONFIG_CMAKE}")
minino_kconfig_enabled(SDR_ENABLE _minino_opt_on)

if(_minino_opt_on)
    idf_component_register()
else()
    idf_component_register(SRCS "surveillance_module.c"
                                    "surveillance_screens.c"
                                    "surveillance_log.c"
                                    "surv_sim_module.c"
                                    "surv_sim_screens.c"
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES surveillance_detect gap_dispatcher radio_lease)
endif()
