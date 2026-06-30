idf_component_register(SRCS  "light_sleep.c" "OTA.c"
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "private_include"
                        REQUIRES
                            driver
                            esp_http_client
                            esp_netif
                            AMOLED
                    )
