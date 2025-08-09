#pragma once

#define AXP2101_ADDRESS                          (0x34)

#define AXP2101_CHIP_ID                          (0x4A)

#define AXP2101_STATUS1                          (0x00)
#define AXP2101_STATUS2                          (0x01)
#define AXP2101_IC_TYPE                          (0x03)


#define AXP2101_DATA_BUFFER1                     (0x04)
#define AXP2101_DATA_BUFFER2                     (0x05)
#define AXP2101_DATA_BUFFER3                     (0x06)
#define AXP2101_DATA_BUFFER4                     (0x07)
#define AXP2101_DATA_BUFFER_SIZE                 (4u)

#define AXP2101_COMMON_CONFIG                    (0x10)
#define AXP2101_BATFET_CTRL                      (0x12)
#define AXP2101_DIE_TEMP_CTRL                    (0x13)
#define AXP2101_MIN_SYS_VOL_CTRL                 (0x14)
#define AXP2101_INPUT_VOL_LIMIT_CTRL             (0x15)
#define AXP2101_INPUT_CUR_LIMIT_CTRL             (0x16)
#define AXP2101_RESET_FUEL_GAUGE                 (0x17)
#define AXP2101_CHARGE_GAUGE_WDT_CTRL            (0x18)


#define AXP2101_WDT_CTRL                         (0x19)
#define AXP2101_LOW_BAT_WARN_SET                 (0x1A)


#define AXP2101_PWRON_STATUS                     (0x20)
#define AXP2101_PWROFF_STATUS                    (0x21)
#define AXP2101_PWROFF_EN                        (0x22)
#define AXP2101_DC_OVP_UVP_CTRL                  (0x23)
#define AXP2101_VOFF_SET                         (0x24)
#define AXP2101_PWROK_SEQU_CTRL                  (0x25)
#define AXP2101_SLEEP_WAKEUP_CTRL                (0x26)
#define AXP2101_IRQ_OFF_ON_LEVEL_CTRL            (0x27)

#define AXP2101_FAST_PWRON_SET0                  (0x28)
#define AXP2101_FAST_PWRON_SET1                  (0x29)
#define AXP2101_FAST_PWRON_SET2                  (0x2A)
#define AXP2101_FAST_PWRON_CTRL                  (0x2B)

#define AXP2101_ADC_CHANNEL_CTRL                 (0x30)
#define AXP2101_ADC_DATA_RELUST0                 (0x34)
#define AXP2101_ADC_DATA_RELUST1                 (0x35)
#define AXP2101_ADC_DATA_RELUST2                 (0x36)
#define AXP2101_ADC_DATA_RELUST3                 (0x37)
#define AXP2101_ADC_DATA_RELUST4                 (0x38)
#define AXP2101_ADC_DATA_RELUST5                 (0x39)
#define AXP2101_ADC_DATA_RELUST6                 (0x3A)
#define AXP2101_ADC_DATA_RELUST7                 (0x3B)
#define AXP2101_ADC_DATA_RELUST8                 (0x3C)
#define AXP2101_ADC_DATA_RELUST9                 (0x3D)


//XPOWERS INTERRUPT REGISTER
#define AXP2101_INTEN1                           (0x40)
#define AXP2101_INTEN2                           (0x41)
#define AXP2101_INTEN3                           (0x42)


//XPOWERS INTERRUPT STATUS REGISTER
#define AXP2101_IRQ_STATUS1                      (0x48)
#define AXP2101_IRQ_STATUS2                      (0x49)
#define AXP2101_IRQ_STATUS3                      (0x4A)
#define AXP2101_IRQ_STATUS_CNT                    (3)

#define AXP2101_TS_PIN_CTRL                      (0x50)
#define AXP2101_TS_HYSL2H_SET                    (0x52)
#define AXP2101_TS_LYSL2H_SET                    (0x53)


#define AXP2101_VLTF_CHG_SET                     (0x54)
#define AXP2101_VHLTF_CHG_SET                    (0x55)
#define AXP2101_VLTF_WORK_SET                    (0x56)
#define AXP2101_VHLTF_WORK_SET                   (0x57)


#define AXP2101_JIETA_EN_CTRL                    (0x58)
#define AXP2101_JIETA_SET0                       (0x59)
#define AXP2101_JIETA_SET1                       (0x5A)
#define AXP2101_JIETA_SET2                       (0x5B)


#define AXP2101_IPRECHG_SET                      (0x61)
#define AXP2101_ICC_CHG_SET                      (0x62)
#define AXP2101_ITERM_CHG_SET_CTRL               (0x63)

#define AXP2101_CV_CHG_VOL_SET                   (0x64)

#define AXP2101_THE_REGU_THRES_SET               (0x65)
#define AXP2101_CHG_TIMEOUT_SET_CTRL             (0x67)

#define AXP2101_BAT_DET_CTRL                     (0x68)
#define AXP2101_CHGLED_SET_CTRL                  (0x69)

#define AXP2101_BTN_VOL_MIN                      (2600)
#define AXP2101_BTN_VOL_MAX                      (3300)
#define AXP2101_BTN_VOL_STEPS                    (100)


#define AXP2101_BTN_BAT_CHG_VOL_SET              (0x6A)


#define AXP2101_DC_ONOFF_DVM_CTRL                (0x80)
#define AXP2101_DC_FORCE_PWM_CTRL                (0x81)
#define AXP2101_DC_VOL0_CTRL                     (0x82)
#define AXP2101_DC_VOL1_CTRL                     (0x83)
#define AXP2101_DC_VOL2_CTRL                     (0x84)
#define AXP2101_DC_VOL3_CTRL                     (0x85)
#define AXP2101_DC_VOL4_CTRL                     (0x86)


#define AXP2101_LDO_ONOFF_CTRL0                  (0x90)
#define AXP2101_LDO_ONOFF_CTRL1                  (0x91)
#define AXP2101_ALDO1_V_CTRL                     (0x92)
#define AXP2101_ALDO2_V_CTRL                     (0x93)
#define AXP2101_ALDO3_V_CTRL                     (0x94)
#define AXP2101_ALDO4_V_CTRL                     (0x95)
#define AXP2101_BLDO1_V_CTRL                     (0x96)
#define AXP2101_BLDO2_V_CTRL                     (0x97)
#define AXP2101_CPUSLDO_V_CTRL                   (0x98)
#define AXP2101_DLDO1_V_CTRL                     (0x99)
#define AXP2101_DLDO2_V_CTRL                     (0x9A)


#define AXP2101_BAT_PARAME                       (0xA1)
#define AXP2101_FUEL_GAUGE_CTRL                  (0xA2)
#define AXP2101_BATTERY_PERCENTAGE               (0xA4)

// DCDC 1~5
#define AXP2101_DCDC1_VOL_MIN                    (1500)
#define AXP2101_DCDC1_VOL_MAX                    (3400)
#define AXP2101_DCDC1_VOL_STEPS                  (100u)

#define AXP2101_DCDC2_VOL1_MIN                   (500u)
#define AXP2101_DCDC2_VOL1_MAX                   (1200u)
#define AXP2101_DCDC2_VOL2_MIN                   (1220u)
#define AXP2101_DCDC2_VOL2_MAX                   (1540u)

#define AXP2101_DCDC2_VOL_STEPS1                 (10u)
#define AXP2101_DCDC2_VOL_STEPS2                 (20u)

#define AXP2101_DCDC2_VOL_STEPS1_BASE            (0u)
#define AXP2101_DCDC2_VOL_STEPS2_BASE            (71)


#define AXP2101_DCDC3_VOL1_MIN                   (500u)
#define AXP2101_DCDC3_VOL1_MAX                   (1200u)
#define AXP2101_DCDC3_VOL2_MIN                   (1220u)
#define AXP2101_DCDC3_VOL2_MAX                   (1540u)
#define AXP2101_DCDC3_VOL3_MIN                   (1600u)
#define AXP2101_DCDC3_VOL3_MAX                   (3400u)

#define AXP2101_DCDC3_VOL_MIN                    (500)
#define AXP2101_DCDC3_VOL_MAX                    (3400)

#define AXP2101_DCDC3_VOL_STEPS1                 (10u)
#define AXP2101_DCDC3_VOL_STEPS2                 (20u)
#define AXP2101_DCDC3_VOL_STEPS3                 (100u)

#define AXP2101_DCDC3_VOL_STEPS1_BASE            (0u)
#define AXP2101_DCDC3_VOL_STEPS2_BASE            (71)
#define AXP2101_DCDC3_VOL_STEPS3_BASE            (88)



#define AXP2101_DCDC4_VOL1_MIN                   (500u)
#define AXP2101_DCDC4_VOL1_MAX                   (1200u)
#define AXP2101_DCDC4_VOL2_MIN                   (1220u)
#define AXP2101_DCDC4_VOL2_MAX                   (1840u)

#define AXP2101_DCDC4_VOL_STEPS1                 (10u)
#define AXP2101_DCDC4_VOL_STEPS2                 (20u)

#define AXP2101_DCDC4_VOL_STEPS1_BASE            (0u)
#define AXP2101_DCDC4_VOL_STEPS2_BASE            (71)



#define AXP2101_DCDC5_VOL_1200MV                 (1200)
#define AXP2101_DCDC5_VOL_VAL                    (0x19)
#define AXP2101_DCDC5_VOL_MIN                    (1400)
#define AXP2101_DCDC5_VOL_MAX                    (3700)
#define AXP2101_DCDC5_VOL_STEPS                  (100u)

#define AXP2101_VSYS_VOL_THRESHOLD_MIN           (2600)
#define AXP2101_VSYS_VOL_THRESHOLD_MAX           (3300)
#define AXP2101_VSYS_VOL_THRESHOLD_STEPS         (100)

// ALDO 1~4

#define AXP2101_ALDO1_VOL_MIN                    (500)
#define AXP2101_ALDO1_VOL_MAX                    (3500)
#define AXP2101_ALDO1_VOL_STEPS                  (100u)

#define AXP2101_ALDO2_VOL_MIN                    (500)
#define AXP2101_ALDO2_VOL_MAX                    (3500)
#define AXP2101_ALDO2_VOL_STEPS                  (100u)


#define AXP2101_ALDO3_VOL_MIN                    (500)
#define AXP2101_ALDO3_VOL_MAX                    (3500)
#define AXP2101_ALDO3_VOL_STEPS                  (100u)


#define AXP2101_ALDO4_VOL_MIN                    (500)
#define AXP2101_ALDO4_VOL_MAX                    (3500)
#define AXP2101_ALDO4_VOL_STEPS                  (100u)

// BLDO 1~2

#define AXP2101_BLDO1_VOL_MIN                    (500)
#define AXP2101_BLDO1_VOL_MAX                    (3500)
#define AXP2101_BLDO1_VOL_STEPS                  (100u)

#define AXP2101_BLDO2_VOL_MIN                    (500)
#define AXP2101_BLDO2_VOL_MAX                    (3500)
#define AXP2101_BLDO2_VOL_STEPS                  (100u)

// CPUSLDO

#define AXP2101_CPUSLDO_VOL_MIN                  (500)
#define AXP2101_CPUSLDO_VOL_MAX                  (1400)
#define AXP2101_CPUSLDO_VOL_STEPS                (50)


// DLDO 1~2
#define AXP2101_DLDO1_VOL_MIN                    (500)
#define AXP2101_DLDO1_VOL_MAX                    (3400)
#define AXP2101_DLDO1_VOL_STEPS                  (100u)

#define AXP2101_DLDO2_VOL_MIN                    (500)
#define AXP2101_DLDO2_VOL_MAX                    (3400)
#define AXP2101_DLDO2_VOL_STEPS                  (100u)


#define AXP2101_CONVERSION(raw)                  (22.0 + (7274 - raw) / 20.0)

