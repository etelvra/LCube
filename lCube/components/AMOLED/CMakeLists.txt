idf_component_register(SRCS  "panel.c"
                        "esp_lcd_co5300.c" "esp_lcd_co5300_spi.c" "esp_lcd_co5300_mipi.c"
                        "esp_lcd_touch.c" "esp_lcd_touch_cst820.c"
                    INCLUDE_DIRS "./includes"
                    REQUIRES
                        driver
                        esp_lcd
                    )
