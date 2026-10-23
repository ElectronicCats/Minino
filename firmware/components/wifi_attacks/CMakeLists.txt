idf_component_register(SRCS "wifi_attacks.c"
PRIV_REQUIRES esp_wifi wifi_controller
INCLUDE_DIRS ".")

# Redirect every reference to ieee80211_raw_frame_sanity_check() to the
# __wrap_ implementation in wifi_attacks.c, so the crafted 802.11 frames the
# attacks inject are not rejected by the ESP-IDF sanity check.
#
# This used to be done by shadowing the symbol and linking with -Wl,-zmuldefs,
# which disabled duplicate-symbol detection for the whole firmware.
target_link_libraries(${COMPONENT_LIB}
                      "-Wl,--wrap=ieee80211_raw_frame_sanity_check")
