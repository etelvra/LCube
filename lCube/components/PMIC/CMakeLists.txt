idf_component_register(SRCS  "AXP2101.c"
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "private_include"
                        REQUIRES
                            driver
                            i2c_bus
                            main
                    )
