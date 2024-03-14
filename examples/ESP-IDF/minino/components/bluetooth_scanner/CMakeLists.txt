set(component_srcs "bluetooth_scanner.c")

idf_component_register(SRCS "${component_srcs}"
                       REQUIRES "bt" "nvs_flash"
                       INCLUDE_DIRS "." "include")
