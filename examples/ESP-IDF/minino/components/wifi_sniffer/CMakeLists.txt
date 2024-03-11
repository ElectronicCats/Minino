set(component_srcs "wifi_sniffer.c")

idf_component_register(SRCS "${component_srcs}"
                        REQUIRES "nvs_flash" "esp_wifi"
                       INCLUDE_DIRS "." "include")
