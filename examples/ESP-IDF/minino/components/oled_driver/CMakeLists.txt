set(component_srcs "oled_driver.c" "oled_driver_i2c.c" "oled_driver_spi.c")

idf_component_register(SRCS "${component_srcs}"
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS ".")
