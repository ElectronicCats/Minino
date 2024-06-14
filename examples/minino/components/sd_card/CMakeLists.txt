idf_component_register(SRCS "sd_card.c"
                    REQUIRES "nvs_flash" "console" "fatfs"
                    INCLUDE_DIRS "include")
