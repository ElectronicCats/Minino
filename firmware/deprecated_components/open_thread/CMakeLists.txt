idf_component_register(SRCS "open_thread.c"
                    INCLUDE_DIRS "include"
                    REQUIRES openthread
                    PRIV_REQUIRES esp_event esp_netif nvs_flash)
