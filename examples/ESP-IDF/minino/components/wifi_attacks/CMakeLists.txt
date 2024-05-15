idf_component_register(SRCS "wifi_attacks.c"
PRIV_REQUIRES esp_wifi wifi_controller
INCLUDE_DIRS ".")
target_link_libraries(${COMPONENT_LIB}  -Wl,-zmuldefs)
