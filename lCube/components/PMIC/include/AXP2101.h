
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
extern bool pmic_monitor;
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

/**
 * @brief 低电量保护配置 (REG 1AH)
 *        当电量低于 warn_pct 时触发警告 IRQ (REG48H[7]),
 *        当电量低于 shutdown_pct 时触发关机 IRQ (REG48H[6]).
 *        同时使能 REG40H 中对应的两个 IRQ.
 *
 * @param warn_pct     警告阈值 (5~20%), 默认 10%
 * @param shutdown_pct 关机阈值 (0~15%), 默认 1%
 */
esp_err_t AXP2101_low_battery_config(uint8_t warn_pct, uint8_t shutdown_pct);

/* ========================================================================
 * PMU Status flags — use with axp2101_status_t.pmu_status_flags (uint16_t)
 * Bit positions mirror REG00H (STATUS1) and REG01H (STATUS2).
 * ======================================================================== */

/* REG00H — STATUS1 (bits 7:0 of pmu_status_flags) */
#define AXP2101_PMU_STS_CURRENT_LIMIT       (1U << 0)   /* In current-limit state */
#define AXP2101_PMU_STS_THERMAL_REG         (1U << 1)   /* In thermal regulation */
#define AXP2101_PMU_STS_BAT_ACTIVE          (1U << 2)   /* Battery in Active Mode */
#define AXP2101_PMU_STS_BAT_PRESENT         (1U << 3)   /* Battery present */
#define AXP2101_PMU_STS_BATFET_OPEN         (1U << 4)   /* BATFET open (1=open) */
#define AXP2101_PMU_STS_VBUS_GOOD           (1U << 5)   /* VBUS good */
/* Bit 7:6 reserved (VBUS good indication multi-level) */

/* REG01H — STATUS2 (bits 15:8 of pmu_status_flags) */
#define AXP2101_PMU_STS_CHG_STAT_SHIFT      8
#define AXP2101_PMU_STS_CHG_STAT_MASK       (7U << 8)   /* REG01[2:0] Charging status */
#define AXP2101_PMU_STS_CHG_STAT_TRI        (0U << 8)   /* 000: Trickle charge */
#define AXP2101_PMU_STS_CHG_STAT_PRE        (1U << 8)   /* 001: Pre-charge */
#define AXP2101_PMU_STS_CHG_STAT_CC         (2U << 8)   /* 010: Constant current */
#define AXP2101_PMU_STS_CHG_STAT_CV         (3U << 8)   /* 011: Constant voltage */
#define AXP2101_PMU_STS_CHG_STAT_DONE       (4U << 8)   /* 100: Charge done */
#define AXP2101_PMU_STS_CHG_STAT_NOT_CHG    (5U << 8)   /* 101: Not charging */

#define AXP2101_PMU_STS_VINDPM              (1U << 11)  /* REG01[3] VINDPM active */
#define AXP2101_PMU_STS_SYS_POWERON         (1U << 12)  /* REG01[4] System power on */

#define AXP2101_PMU_STS_BAT_DIR_SHIFT       13
#define AXP2101_PMU_STS_BAT_DIR_MASK        (3U << 13)  /* REG01[6:5] Bat current direction */
#define AXP2101_PMU_STS_BAT_DIR_STANDBY     (0U << 13)  /* 00: Standby */
#define AXP2101_PMU_STS_BAT_DIR_CHARGE      (1U << 13)  /* 01: Charge */
#define AXP2101_PMU_STS_BAT_DIR_DISCHARGE   (2U << 13)  /* 10: Discharge */
/* Bit 15 reserved (REG01[7]) */

/* ========================================================================
 * IRQ Status flags — use with axp2101_status_t.irq_status_flags (uint32_t)
 * Bit positions mirror REG48H (IRQ_STATUS1), REG49H (IRQ_STATUS2),
 * REG4AH (IRQ_STATUS3).
 * ======================================================================== */

/* REG48H — IRQ_STATUS1 (bits 7:0 of irq_status_flags) */
#define AXP2101_IRQ_STS_BAT_WORK_UNDER_TEMP (1UL << 0)  /* REG48[0] */
#define AXP2101_IRQ_STS_BAT_WORK_OVER_TEMP  (1UL << 1)  /* REG48[1] */
#define AXP2101_IRQ_STS_BAT_CHG_UNDER_TEMP  (1UL << 2)  /* REG48[2] */
#define AXP2101_IRQ_STS_BAT_CHG_OVER_TEMP   (1UL << 3)  /* REG48[3] */
#define AXP2101_IRQ_STS_GAUGE_NEW_SOC       (1UL << 4)  /* REG48[4] */
#define AXP2101_IRQ_STS_GAUGE_WDT_TIMEOUT   (1UL << 5)  /* REG48[5] */
#define AXP2101_IRQ_STS_SOC_SHUTDOWN        (1UL << 6)  /* REG48[6] */
#define AXP2101_IRQ_STS_SOC_WARNING         (1UL << 7)  /* REG48[7] */

/* REG49H — IRQ_STATUS2 (bits 15:8 of irq_status_flags) */
#define AXP2101_IRQ_STS_PWRON_POS_EDGE      (1UL << 8)  /* REG49[0] */
#define AXP2101_IRQ_STS_PWRON_NEG_EDGE      (1UL << 9)  /* REG49[1] */
#define AXP2101_IRQ_STS_PWRON_LONG_PRESS    (1UL << 10) /* REG49[2] */
#define AXP2101_IRQ_STS_PWRON_SHORT_PRESS   (1UL << 11) /* REG49[3] */
#define AXP2101_IRQ_STS_BAT_REMOVE          (1UL << 12) /* REG49[4] */
#define AXP2101_IRQ_STS_BAT_INSERT          (1UL << 13) /* REG49[5] */
#define AXP2101_IRQ_STS_VBUS_REMOVE         (1UL << 14) /* REG49[6] */
#define AXP2101_IRQ_STS_VBUS_INSERT         (1UL << 15) /* REG49[7] */

/* REG4AH — IRQ_STATUS3 (bits 23:16 of irq_status_flags) */
#define AXP2101_IRQ_STS_BAT_OVP             (1UL << 16) /* REG4A[0] */
#define AXP2101_IRQ_STS_CHG_TIMER_EXPIRE    (1UL << 17) /* REG4A[1] */
#define AXP2101_IRQ_STS_DIE_OVERTEMP_L1     (1UL << 18) /* REG4A[2] */
#define AXP2101_IRQ_STS_CHARGE_START        (1UL << 19) /* REG4A[3] */
#define AXP2101_IRQ_STS_CHARGE_DONE         (1UL << 20) /* REG4A[4] */
#define AXP2101_IRQ_STS_BATFET_OCP          (1UL << 21) /* REG4A[5] */
#define AXP2101_IRQ_STS_LDO_OC              (1UL << 22) /* REG4A[6] */
#define AXP2101_IRQ_STS_WDT_EXPIRE          (1UL << 23) /* REG4A[7] */

/*  DCDC power rail control  */
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

/*  LDO power rail control  */

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


/** Single power-rail state (DCDC or LDO) */
typedef struct {
    bool     enabled;
    uint16_t voltage_mv;
} axp2101_power_rail_t;

typedef struct axp2101_status{
    /*  ADC measurements (REG30 enable, REG34~3D data)  */
    uint16_t vbat_mv;         //< Battery voltage (REG34/35)
    uint16_t ts_mv;           //< TS pin ADC raw (REG36/37), 0.5mV/LSB
    uint16_t vbus_mv;         //< VBUS voltage  (REG38/39)
    uint16_t vsys_mv;         //< VSYS voltage  (REG3A/3B)
    uint16_t die_temp;        //< Die temperature in 0.1 °C (REG3C/3D converted)

    /*  Die OTP threshold (REG13[2:1])  */
    uint8_t die_otp_threshold; //< 115, 125, or 135 °C

    uint8_t  battery_pct;     //< Battery percentage 0~100% (REGA4)

    /*  Power-on / off source (REG20 / REG21)  */
    uint8_t pwr_on_source;    //< Raw REG20 value
    uint8_t pwr_off_source;   //< Raw REG21 value

    /*  PMU Status (REG00, REG01)  */
    uint16_t pmu_status_flags;
    /*  IRQ Status (REG48, REG49, REG4A)  */
    uint32_t irq_status_flags;

    /*  Charger config readback (REG61~64)  */
    uint8_t iprechg_ma;      //< Pre-charge current limit (REG61)
    uint8_t ichg_ma0;         //< CC charge current limit (REG62)
    uint8_t iterm_ma;        //< Termination current limit (REG63)
    uint8_t cv_voltage_mv00;   //< Target charge voltage (REG64)

    /*  Power rails  */
    axp2101_power_rail_t dcdc[5]; //< DCDC1~5  (REG80, REG82~86)
    axp2101_power_rail_t ldo[9];  //< ALDO1~4, BLDO1~2, CPUSLDO, DLDO1~2 (REG90~91, REG92~9A)

} axp2101_status_t;


void PMIC_status_list_refresh(const axp2101_status_t *axp2101_status);

/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* PMIC_AXP2101_H_ */
