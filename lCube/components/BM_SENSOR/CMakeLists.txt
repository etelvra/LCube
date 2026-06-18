idf_component_register(SRCS  "BM_SENSOR.c"
                        "bmi2_ois.c" "bmi2.c" "bmi270_context.c" "bmi270_legacy.c" "bmi270_maximum_fifo.c" "bmi270.c" "common/common.c"
                        "./BMP280/bmp280.c"
                        "./BMM150/bmm150.c"  "./BMM150/bmm150_aux_adapter.c"
                        INCLUDE_DIRS "./include"
                        PRIV_INCLUDE_DIRS "./private_include" "./common" "./BMP280" "./BMM150"
                        REQUIRES
                            driver
                            i2c_bus
                            PMIC
                    )
