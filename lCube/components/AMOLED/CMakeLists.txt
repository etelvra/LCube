file(GLOB_RECURSE SRC_UI ${CMAKE_SOURCE_DIR} "ui/*.c")

idf_component_register(SRCS  "panel.c" "lvgl_display.c"
                        "private_include/esp_lcd_co5300.c" "private_include/esp_lcd_co5300_spi.c" "private_include/esp_lcd_co5300_mipi.c"
                        "private_include/esp_lcd_touch.c" "private_include/esp_lcd_touch_cst820.c"
                        ${SRC_UI}
                    INCLUDE_DIRS "./include" "./ui"
                    PRIV_INCLUDE_DIRS "./private_include"
                    REQUIRES
                        driver
                        esp_lcd
                        main
                        PMIC
                    )
