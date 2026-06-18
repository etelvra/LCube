idf_component_register(SRCS  "MIC.c"
                        INCLUDE_DIRS "include"
                        #PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES
                        driver
                        adc_mic
                        esp_codec_dev
                        esp-dsp
                        AMOLED
                    )
