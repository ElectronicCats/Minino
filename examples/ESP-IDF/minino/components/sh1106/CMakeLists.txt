set(component_srcs "sh1106.c" "sh1106_i2c.c" "sh1106_spi.c")

idf_component_register(SRCS "${component_srcs}"
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS ".")
