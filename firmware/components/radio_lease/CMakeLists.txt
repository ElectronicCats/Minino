idf_component_register(SRCS "radio_lease_registry.c"
                            "radio_lease.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_wifi bt esp_driver_uart wifi_controller
                                  wifi_service gap_dispatcher)
