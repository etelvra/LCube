idf_component_register(SRCS  "TimeStamp.c" "WIFI.c" "TCP_client.c" "Mood_sync.c"
                    INCLUDE_DIRS "./includes"
                    REQUIRES
                        driver
                        esp_wifi
                        nvs_flash
                        esp_netif
                        AMOLED
                    )
target_link_options(${COMPONENT_TARGET} INTERFACE "-Wl,-zmuldefs")
