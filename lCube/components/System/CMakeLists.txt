idf_component_register(SRCS  "light_sleep.c" "OTA.c" "FatFS.c" "usb_msc.c"
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "private_include"
                        REQUIRES
                            driver
                            esp_http_client
                            esp_netif
                            vfs
                            tinyusb
                            wear_levelling
                            AMOLED
                    )
