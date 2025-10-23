idf_component_register(SRCS "nmea_parser.c"
                    PRIV_REQUIRES esp_event driver main
                    INCLUDE_DIRS "include")
