# WiFi Service singleton: reemplaza 13 esp_wifi_init() en todo el árbol.
# Depende de esp_wifi.

idf_component_register(
  SRCS "wifi_service.c"
  INCLUDE_DIRS "."
  REQUIRES esp_wifi
)