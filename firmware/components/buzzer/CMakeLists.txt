set(component_srcs "buzzer.c")

idf_component_register(SRCS "${component_srcs}"
                       REQUIRES "driver" "esp_timer"
                       INCLUDE_DIRS "." "include")
