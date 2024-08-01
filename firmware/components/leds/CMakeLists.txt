idf_component_register(SRCS "leds.c"
                    REQUIRES "driver" ledc_controller
                    INCLUDE_DIRS "include")
