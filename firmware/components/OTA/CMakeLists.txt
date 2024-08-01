idf_component_register(SRCS "OTA.c" 
                            "modules/http_server/http_server.c" 
                            "modules/wifi_ap/wifi_ap.c"
                    INCLUDE_DIRS "include" "modules/http_server" "modules/wifi_ap"
                    PRIV_REQUIRES esp_wifi esp_http_server app_update esp_timer nvs_flash
                    EMBED_FILES src/webpage/app.css src/webpage/app.js src/webpage/favicon.ico 
                    src/webpage/index.html src/webpage/jquery-3.3.1.min.js
                    )
