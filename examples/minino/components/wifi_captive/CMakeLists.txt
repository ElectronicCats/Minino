idf_component_register(SRCS "captive_portal.c"
PRIV_REQUIRES esp_wifi esp_netif nvs_flash esp_http_server dns_server
INCLUDE_DIRS ".")
