set(component_srcs "wifi_sniffer.c" "cmd_pcap.c" "cmd_sniffer.c")

idf_component_register(SRCS "${component_srcs}"
                       REQUIRES "nvs_flash" "esp_wifi" "app_trace" "console" "fatfs"
                       INCLUDE_DIRS "." "include")
