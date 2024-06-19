set(component_srcs "wifi_controller.c")

# get IDF version for comparison
set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

idf_component_register(SRCS "${component_srcs}" PRIV_REQUIRES esp_wifi INCLUDE_DIRS ".")
