include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/minino_kconfig.cmake")
minino_kconfig_enabled(MININO_BT_STACK _minino_opt_on)

if(_minino_opt_on)
    idf_component_register(SRCS "alt_unix_time/alt_unix_time.c"
                            "id_open/id_open.cpp"
                            "id_open_esp32/id_open_beacon.cpp"
                            "id_open_esp32/id_open_esp32.cpp"
                            "odid_wifi/wifi.c"
                            "odrone_id.cpp"
                            "opendroneid/opendroneid.c"
                            "spoofer/spoofer.cpp"
                            "utm/utm.cpp"
                        INCLUDE_DIRS "."
                                     "alt_unix_time"
                                     "id_open"
                                     "id_open_esp32"
                                     "odid_wifi"
                                     "opendroneid"
                                     "spoofer"
                                     "utm"
                        PRIV_REQUIRES esp_wifi nvs_flash bt radio_lease)
else()
    idf_component_register()
endif()