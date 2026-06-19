
#ifndef PMIC_AXP2101_H_
#define PMIC_AXP2101_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/*!             Header files
 ******************************************************************************/
#include "AXP2101.h"
#include "i2c_bus.h"

extern i2c_bus_handle_t i2c_bus_handle;
extern bool i2c_bus_initialized;
esp_err_t i2c_bus_init(void);
void PMIC_init(void);
uint8_t AXP2101_bat_percentage(void);
esp_err_t AXP2101_amoled_turn_off(void);
esp_err_t AXP2101_sys_shutdown(void);

/**
 * @brief 充电参数初始化
 * @param chg_current_ma    恒流充电电流 (mA), 例: 200 (0.5C of 400mAh)
 * @param chg_voltage_mv    充电目标电压 (mV), 例: 4200
 * @param prechg_current_ma 预充电电流 (mA), 例: 100
 * @param term_current_ma   充电终止电流 (mA), 例: 25 (≈0.05C of 400mAh)
 * @return esp_err_t        ESP_OK on success
 */
esp_err_t AXP2101_charger_init(uint16_t chg_current_ma, uint16_t chg_voltage_mv,
                                uint16_t prechg_current_ma, uint16_t term_current_ma);


/* ===== DCDC power rail control ===== */
/* DCDC channel identifiers (REG80H bit positions) */
typedef enum {
    AXP2101_DCDC1 = 1,
    AXP2101_DCDC2 = 2,
    AXP2101_DCDC3 = 3,
    AXP2101_DCDC4 = 4,
    AXP2101_DCDC5 = 5,
} axp2101_dcdc_channel_t;

/* LDO channel identifiers */
typedef enum {
    AXP2101_ALDO1 = 0,
    AXP2101_ALDO2,
    AXP2101_ALDO3,
    AXP2101_ALDO4,
    AXP2101_BLDO1,
    AXP2101_BLDO2,
    AXP2101_CPUSLDO,
    AXP2101_DLDO1,
    AXP2101_DLDO2,
} axp2101_ldo_channel_t;

/**
 * @brief Enable or disable a DCDC channel
 * @param dcdc_ch  DCDC channel (AXP2101_DCDC1 ~ AXP2101_DCDC5)
 * @param enable   true = enable, false = disable
 */
esp_err_t AXP2101_dcdc_enable(axp2101_dcdc_channel_t dcdc_ch, bool enable);

/**
 * @brief Set DCDC output voltage
 * @param dcdc_ch     DCDC channel
 * @param voltage_mv  Target voltage in mV
 *                    DCDC1: 1500~3400 (100mV/step)
 *                    DCDC2: 500~1200 (10mV), 1220~1540 (20mV)
 *                    DCDC3: 500~1200 (10mV), 1220~1540 (20mV), 1600~3400 (100mV)
 *                    DCDC4: 500~1200 (10mV), 1220~1840 (20mV)
 *                    DCDC5: 1200, 1400~3700 (100mV)
 */
esp_err_t AXP2101_dcdc_set_voltage(axp2101_dcdc_channel_t dcdc_ch, uint16_t voltage_mv);

/**
 * @brief Set DCDC PWM mode (force PWM or auto PFM)
 * @param dcdc_ch    DCDC channel (DCDC1~4 only, DCDC5 not supported)
 * @param force_pwm  true = always PWM, false = auto PFM/PWM
 */
esp_err_t AXP2101_dcdc_set_pwm_mode(axp2101_dcdc_channel_t dcdc_ch, bool force_pwm);

/**
 * @brief Enable DVM (Dynamic Voltage Management) for DCDC2/3
 *        When enabled, voltage changes occur step-by-step at the configured rate.
 * @param dcdc_ch  DCDC channel (DCDC2 or DCDC3 only)
 * @param enable   true = enable DVM, false = disable
 */
esp_err_t AXP2101_dcdc_set_dvm(axp2101_dcdc_channel_t dcdc_ch, bool enable);

/* ===== LDO power rail control ===== */

/**
 * @brief Enable or disable an LDO channel
 * @param ldo_ch  LDO channel
 * @param enable  true = enable, false = disable
 */
esp_err_t AXP2101_ldo_enable(axp2101_ldo_channel_t ldo_ch, bool enable);

/**
 * @brief Set LDO output voltage
 * @param ldo_ch      LDO channel
 * @param voltage_mv  Target voltage in mV
 *                    ALDO1~4, BLDO1~2: 500~3500 (100mV/step)
 *                    CPUSLDO:          500~1400 (50mV/step)
 *                    DLDO1:            500~3300 (100mV/step)
 *                    DLDO2:            500~1400 (50mV/step)
 */
esp_err_t AXP2101_ldo_set_voltage(axp2101_ldo_channel_t ldo_ch, uint16_t voltage_mv);

/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* PMIC_AXP2101_H_ */
