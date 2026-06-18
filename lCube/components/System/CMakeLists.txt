idf_component_register(SRCS  "light_sleep.c"
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "private_include"
                        REQUIRES
                            driver
                            main
                            AMOLED
                    )
