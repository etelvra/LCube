#pragma once
//PCB Pin definition

//I2C
#define IOPIN_I2C_SCL                   (GPIO_NUM_17)
#define IOPIN_I2C_SDA                   (GPIO_NUM_18)
//QSPI  (4-bit) Quad SPI
#define LCD_HOST                        (SPI2_HOST)
#define IOPIN_QSPI_CS0                  (GPIO_NUM_10)
#define IOPIN_QSPI_CLK                  (GPIO_NUM_12)
#define IOPIN_QSPI_D_0                  (GPIO_NUM_11)
#define IOPIN_QSPI_Q_1                  (GPIO_NUM_13)
#define IOPIN_QSPI_WP_2                 (GPIO_NUM_14)
#define IOPIN_QSPI_HD_3                 (GPIO_NUM_9)
//AMOLED
#define IOPIN_TP_IRQ                    (GPIO_NUM_5)
#define IOPIN_TP_RST                    (GPIO_NUM_4)
#define IOPIN_LCD_RST                   (GPIO_NUM_7)
#define IOPIN_LCD_TE                    (GPIO_NUM_8)

//ADC
#define IOPIN_ADC_MIC                   (GPIO_NUM_1)
//PMIC AXP2101
#define IOPIN_PMIC_PWR                  (GPIO_NUM_3)
#define IOPIN_PMIC_IRQ                  (GPIO_NUM_21)
//LED
#define IOPIN_LED_BLUE                  (GPIO_NUM_0)
#define IOPIN_LED_WHITE                 (GPIO_NUM_41)

//RMT
#define IOPIN_RMT_TX                    (GPIO_NUM_2)
#define IOPIN_RMT_RX                    (GPIO_NUM_48)

