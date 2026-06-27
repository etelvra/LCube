idf_component_register(SRCS  "TimeStamp.c" "HTTP.c" "OTA.c" "WIFI.c"
                    INCLUDE_DIRS "./includes"
                    REQUIRES
                        driver
                        esp_wifi
                        nvs_flash
                        esp_http_client
                        esp-tls
                        esp_netif
                        AMOLED
                    )
target_link_options(${COMPONENT_TARGET} INTERFACE "-Wl,-zmuldefs")
