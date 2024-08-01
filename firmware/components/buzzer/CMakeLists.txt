set(component_srcs "buzzer.c")

idf_component_register(SRCS "${component_srcs}"
                       REQUIRES "driver"
                       INCLUDE_DIRS "." "include")
