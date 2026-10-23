include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_OPENTHREAD_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "thread_broadcast.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES open_thread)
else()
    idf_component_register()
endif()