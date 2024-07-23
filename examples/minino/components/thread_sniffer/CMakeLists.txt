idf_component_register(SRCS "thread_sniffer.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES espressif__pcap sd_card
                    REQUIRES open_thread)
