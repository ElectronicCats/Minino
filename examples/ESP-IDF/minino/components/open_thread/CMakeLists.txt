idf_component_register(SRCS "open_thread.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_event esp_netif openthread nvs_flash)
