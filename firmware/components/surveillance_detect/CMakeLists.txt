idf_component_register(SRCS "surv_signatures.c"
                            "surv_match.c"
                            "surv_ie.c"
                            "surv_overlay.c"
                            "surv_engine.c"
                            "surv_radio.c"
                            "surveillance_detect.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_wifi bt esp_timer radio_selector)
