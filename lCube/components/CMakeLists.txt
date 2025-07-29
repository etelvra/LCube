idf_component_register(SRCS "esp_lcd_touch.c" "esp_lcd_touch_cst820.c" "AXP2101.c"  "LED.c"  "WIFI.c"  "HTTP.c"  "TimeStamp.c"
                            "esp_lcd_co5300.c" "esp_lcd_co5300_spi.c" "esp_lcd_co5300_mipi.c"
                    INCLUDE_DIRS "./includes"
                    REQUIRES
                        driver
                        esp_wifi
                        nvs_flash
                        esp_http_client
                        esp-tls
                        esp_netif
                        esp_lcd
                    )
