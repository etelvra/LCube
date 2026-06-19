#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_check.h"
#include "driver/gpio.h"

#include "Pin_Definitions.h"
#include "AXP2101Constants.h"
#include "AXP2101.h"
#include "light_sleep.h"
#include "panel.h"

static const char* TAG = "PMIC";

bool i2c_bus_initialized = false;
i2c_bus_handle_t i2c_bus_handle = NULL;
static i2c_bus_device_handle_t axp2101_i2c_device_handle = NULL;
static QueueHandle_t pmic_event_queue = NULL;

#define AXP2101_I2C_RETRY_COUNT     3
#define AXP2101_I2C_RETRY_DELAY_MS   1

static esp_err_t axp2101_i2c_read_bytes(uint8_t mem_address, size_t data_len, uint8_t *data) {
    esp_err_t ret = ESP_FAIL;
    for (int i = 0; i < AXP2101_I2C_RETRY_COUNT; i++) {
        ret = i2c_bus_read_bytes(axp2101_i2c_device_handle, mem_address, data_len, data);
        if (ret == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(AXP2101_I2C_RETRY_DELAY_MS));
    }
    ESP_LOGE(TAG, "I2C read 0x%02X failed after %d retries", mem_address, AXP2101_I2C_RETRY_COUNT);
    return ret;
}

static esp_err_t axp2101_i2c_write_bytes(uint8_t mem_address, size_t data_len, const uint8_t *data) {
    esp_err_t ret = ESP_FAIL;
    for (int i = 0; i < AXP2101_I2C_RETRY_COUNT; i++) {
        ret = i2c_bus_write_bytes(axp2101_i2c_device_handle, mem_address, data_len, data);
        if (ret == ESP_OK) return ESP_OK;
        vTaskDelay(pdMS_TO_TICKS(AXP2101_I2C_RETRY_DELAY_MS));
    }
    ESP_LOGE(TAG, "I2C write 0x%02X failed after %d retries", mem_address, AXP2101_I2C_RETRY_COUNT);
    return ret;
}

//Create the interrupt service function
static void IRAM_ATTR PMIC_IRQfunction_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(lightsleep_event_queue, &gpio_num, NULL);
    xQueueSendFromISR(pmic_event_queue, &gpio_num, NULL);
}
static void task_pmic_management(void *param);
static uint32_t AXP2101_check_status(void);

void PMIC_init(void)
{
    i2c_bus_init();
    axp2101_i2c_device_handle = i2c_bus_device_create(i2c_bus_handle, AXP2101_ADDRESS, 0);

    //configure GPIO for axp2101 interrupt
    const gpio_config_t PMIC_irq_config = {
        .pin_bit_mask = (1ULL<<IOPIN_PMIC_IRQ),//| (1ULL<<IOPIN_PMIC_PWR),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .mode         = GPIO_MODE_INPUT,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&PMIC_irq_config);
    gpio_set_intr_type(IOPIN_PMIC_IRQ, GPIO_INTR_NEGEDGE);

    //create a queue to handle gpio event from isr
    pmic_event_queue = xQueueCreate(16, sizeof(uint32_t));
    if (lightsleep_event_queue == NULL){
        lightsleep_event_queue = xQueueCreate(10, sizeof(uint32_t));
    }
    //Configure only once to install gpio isr service(high priority)for the project
    gpio_install_isr_service(ESP_INTR_FLAG_LOWMED|ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_EDGE);

    //hook isr handler for specific gpio pin
//    gpio_isr_handler_add(IOPIN_PMIC_PWR, PMIC_IRQfunction_handler, (void*) IOPIN_PMIC_PWR);
    gpio_isr_handler_add(IOPIN_PMIC_IRQ, PMIC_IRQfunction_handler, (void*) IOPIN_PMIC_IRQ);

    xTaskCreatePinnedToCore(task_pmic_management,"task_pwr_management",4096,NULL,20,NULL,0);

//    gpio_isr_handler_remove(IOPIN_PMIC_PWR);
//    gpio_uninstall_isr_service();
}

esp_err_t i2c_bus_init(void)
{
    /*IF I2C was initialized before */
    if (!i2c_bus_initialized) {
    const i2c_config_t i2c_bus_config = {
        .mode = I2C_MODE_MASTER,
        .clk_flags = I2C_CLK_SRC_DEFAULT,
        .sda_io_num    = IOPIN_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num    = IOPIN_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400*1000,
    };
    i2c_bus_handle = i2c_bus_create(I2C_BUS_PORT, &i2c_bus_config);

    i2c_bus_initialized = true;
    }
    return ESP_OK;
}

static void task_pmic_management(void *param)
{
    esp_err_t ret;
    ret = AXP2101_low_battery_config(20, 10);
    if (ret != ESP_OK) goto init_fail;

    ret = AXP2101_charger_init(200, 4200, 100, 25);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "charger init failed, continuing without charger config");
    }

    ret = AXP2101_dcdc_set_voltage(AXP2101_DCDC1, 3300);
    if (ret != ESP_OK) ESP_LOGW(TAG, "DCDC1 init_fail");
    ret = AXP2101_dcdc_enable(AXP2101_DCDC1, true);
    if (ret != ESP_OK) ESP_LOGW(TAG, "DCDC1 init_fail");
    AXP2101_dcdc_enable(AXP2101_DCDC2, false);
    AXP2101_dcdc_enable(AXP2101_DCDC3, false);
    AXP2101_dcdc_enable(AXP2101_DCDC4, false);
    AXP2101_dcdc_enable(AXP2101_DCDC5, false);

    ret = AXP2101_ldo_set_voltage(AXP2101_ALDO3, 3300);
    if (ret != ESP_OK) ESP_LOGW(TAG, "LDO init_fail");
    ret = AXP2101_ldo_enable(AXP2101_ALDO3, true);
    if (ret != ESP_OK) ESP_LOGW(TAG, "LDO init_fail");

    uint8_t status[2];
    ret = axp2101_i2c_read_bytes(AXP2101_STATUS1, 2, status);
    ESP_LOGI(TAG, "AXP2101_status is %02x %02x", status[0], status[1]);

    AXP2101_check_status(); /* clear any pending IRQ before entering main loop */

    uint32_t io_num;
    uint8_t bat_pct;
    uint32_t pmic_event_flags;

    while (1) {
        if (xQueueReceive(pmic_event_queue, &io_num, portMAX_DELAY)) {
            bat_pct = AXP2101_bat_percentage();
            pmic_event_flags = AXP2101_check_status();
            if (bat_pct != 0xFF && bat_pct <= 10) {
                AXP2101_sys_shutdown();
            } else if (bat_pct != 0xFF && bat_pct <= 20
                       && !(pmic_event_flags & AXP2101_STATUS_VBUS_GOOD)) {
                AXP2101_sys_shutdown();
            }
            AMOLED_console_log(INFORM, false, TAG, "GPIO[%"PRIu32"] intr, val: %d\n", io_num, gpio_get_level(io_num));
            AMOLED_console_log(INFORM, false, TAG, "bat percentage is %d\n", bat_pct);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

init_fail:
    ESP_LOGE(TAG, "PMIC init I2C failed, task exit");
    AXP2101_sys_shutdown();
    vTaskDelete(NULL);
}

uint8_t AXP2101_bat_percentage(void)
{
    uint8_t data;
    esp_err_t ret;

    ret = axp2101_i2c_read_bytes(AXP2101_STATUS1, 1, &data);
    if (ret != ESP_OK) return 0xFF;

    if (data & (1 << 3)) {
        ret = axp2101_i2c_read_bytes(AXP2101_BATTERY_PERCENTAGE, 1, &data);
        if (ret != ESP_OK) return 0xFF;
    } else {
        return 0;
    }
    return data;
}

esp_err_t AXP2101_amoled_turn_off(void)
{
    return AXP2101_ldo_enable(AXP2101_ALDO3, false);
}

esp_err_t AXP2101_sys_shutdown(void)
{
    uint8_t data;
    ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(AXP2101_COMMON_CONFIG, 1, &data),
                        TAG, "read COMMON_CONFIG before shutdown failed");
    data |= (1 << 0);
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_COMMON_CONFIG, 1, &data),
                        TAG, "write shutdown command failed");
    return ESP_OK;
}

esp_err_t AXP2101_charger_init(uint16_t chg_current_ma, uint16_t chg_voltage_mv,
                                uint16_t prechg_current_ma, uint16_t term_current_ma)
{
    uint8_t data;
    //REG 18H: Gauge Module enable and Cell Battery charge enable(This register is set as default)
    data = (1 << 3) | (1 << 1);
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_CHARGE_GAUGE_WDT_CTRL, 1, &data),
                        TAG, "write CHARGE_GAUGE_WDT_CTRL failed");
    //REG 61H: Precharge current limit, 25mA/step, 0~200mA
    data = prechg_current_ma / 25;
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_IPRECHG_SET, 1, &data),
                        TAG, "write IPRECHG_SET failed");
    //REG 62H: Constant current charge current limit
    if (chg_current_ma <= 200) {
        data = chg_current_ma / 25;
    } else {
        data = 8 + (chg_current_ma - 200) / 100;
    }
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_ICC_CHG_SET, 1, &data),
                        TAG, "write ICC_CHG_SET failed");
    //REG 63H: Charging termination of current enable | Termination current limit(25mA/step)
    data = (1 << 4) | (term_current_ma / 25);
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_ITERM_CHG_SET_CTRL, 1, &data),
                        TAG, "write ITERM_CHG_SET_CTRL failed");
    //REG 64H: Charge voltage limit
    switch (chg_voltage_mv) {
    case 4000: data = 0x01; break;
    case 4100: data = 0x02; break;
    case 4200: data = 0x03; break;
    case 4350: data = 0x04; break;
    case 4400: data = 0x05; break;
    default:   data = 0x03; break;
    }
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_CV_CHG_VOL_SET, 1, &data),
                        TAG, "write CV_CHG_VOL_SET failed");
    //REG 67H: Charger timeout setting and control(default)
    data = (1 << 7) | (1 << 6) | (1 << 2) | 0x10;
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_CHG_TIMEOUT_SET_CTRL, 1, &data),
                        TAG, "write CHG_TIMEOUT_SET_CTRL failed");

    return ESP_OK;
}

esp_err_t AXP2101_low_battery_config(uint8_t warn_pct, uint8_t shutdown_pct)
{
    if (warn_pct < 5 || warn_pct > 20 || shutdown_pct > 15) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data = ((warn_pct - 5) << 4) | shutdown_pct;
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_LOW_BAT_WARN_SET, 1, &data),
                        TAG, "write LOW_BAT_WARN_SET failed");

    ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(AXP2101_INTEN1, 1, &data),
                        TAG, "read INTEN1 failed");
    data |= (1 << 7) | (1 << 6);
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_INTEN1, 1, &data),
                        TAG, "write INTEN1 failed");

    return ESP_OK;
}

/* ===== DCDC power rail control ===== */

esp_err_t AXP2101_dcdc_enable(axp2101_dcdc_channel_t dcdc_ch, bool enable)
{
    if (dcdc_ch < AXP2101_DCDC1 || dcdc_ch > AXP2101_DCDC5) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data;
    ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(AXP2101_DC_ONOFF_DVM_CTRL, 1, &data),
                        TAG, "read DC_ONOFF_DVM_CTRL failed");

    if (enable) {
        data |= (1 << (dcdc_ch - 1));
    } else {
        data &= ~(1 << (dcdc_ch - 1));
    }

    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_DC_ONOFF_DVM_CTRL, 1, &data),
                        TAG, "write DC_ONOFF_DVM_CTRL failed");
    return ESP_OK;
}

esp_err_t AXP2101_dcdc_set_voltage(axp2101_dcdc_channel_t dcdc_ch, uint16_t voltage_mv)
{
    uint8_t reg_addr;
    uint8_t reg_val;
    uint8_t cur;

    switch (dcdc_ch) {
    case AXP2101_DCDC1:
        if (voltage_mv < AXP2101_DCDC1_VOL_MIN || voltage_mv > AXP2101_DCDC1_VOL_MAX) {
            return ESP_ERR_INVALID_ARG;
        }
        reg_addr = AXP2101_DC_VOL0_CTRL;
        reg_val = (voltage_mv - AXP2101_DCDC1_VOL_MIN) / AXP2101_DCDC1_VOL_STEPS;
        return axp2101_i2c_write_bytes(reg_addr, 1, &reg_val);

    case AXP2101_DCDC2:
        reg_addr = AXP2101_DC_VOL1_CTRL;
        if (voltage_mv >= AXP2101_DCDC2_VOL1_MIN && voltage_mv <= AXP2101_DCDC2_VOL1_MAX) {
            reg_val = AXP2101_DCDC2_VOL_STEPS1_BASE + (voltage_mv - AXP2101_DCDC2_VOL1_MIN) / AXP2101_DCDC2_VOL_STEPS1;
        } else if (voltage_mv >= AXP2101_DCDC2_VOL2_MIN && voltage_mv <= AXP2101_DCDC2_VOL2_MAX) {
            reg_val = AXP2101_DCDC2_VOL_STEPS2_BASE + (voltage_mv - AXP2101_DCDC2_VOL2_MIN) / AXP2101_DCDC2_VOL_STEPS2;
        } else {
            return ESP_ERR_INVALID_ARG;
        }
        ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(reg_addr, 1, &cur), TAG, "read DCDC2 vol failed");
        reg_val |= (cur & 0x80);
        return axp2101_i2c_write_bytes(reg_addr, 1, &reg_val);

    case AXP2101_DCDC3:
        reg_addr = AXP2101_DC_VOL2_CTRL;
        if (voltage_mv >= AXP2101_DCDC3_VOL1_MIN && voltage_mv <= AXP2101_DCDC3_VOL1_MAX) {
            reg_val = AXP2101_DCDC3_VOL_STEPS1_BASE + (voltage_mv - AXP2101_DCDC3_VOL1_MIN) / AXP2101_DCDC3_VOL_STEPS1;
        } else if (voltage_mv >= AXP2101_DCDC3_VOL2_MIN && voltage_mv <= AXP2101_DCDC3_VOL2_MAX) {
            reg_val = AXP2101_DCDC3_VOL_STEPS2_BASE + (voltage_mv - AXP2101_DCDC3_VOL2_MIN) / AXP2101_DCDC3_VOL_STEPS2;
        } else if (voltage_mv >= AXP2101_DCDC3_VOL3_MIN && voltage_mv <= AXP2101_DCDC3_VOL3_MAX) {
            reg_val = AXP2101_DCDC3_VOL_STEPS3_BASE + (voltage_mv - AXP2101_DCDC3_VOL3_MIN) / AXP2101_DCDC3_VOL_STEPS3;
        } else {
            return ESP_ERR_INVALID_ARG;
        }
        ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(reg_addr, 1, &cur), TAG, "read DCDC3 vol failed");
        reg_val |= (cur & 0x80);
        return axp2101_i2c_write_bytes(reg_addr, 1, &reg_val);

    case AXP2101_DCDC4:
        reg_addr = AXP2101_DC_VOL3_CTRL;
        if (voltage_mv >= AXP2101_DCDC4_VOL1_MIN && voltage_mv <= AXP2101_DCDC4_VOL1_MAX) {
            reg_val = AXP2101_DCDC4_VOL_STEPS1_BASE + (voltage_mv - AXP2101_DCDC4_VOL1_MIN) / AXP2101_DCDC4_VOL_STEPS1;
        } else if (voltage_mv >= AXP2101_DCDC4_VOL2_MIN && voltage_mv <= AXP2101_DCDC4_VOL2_MAX) {
            reg_val = AXP2101_DCDC4_VOL_STEPS2_BASE + (voltage_mv - AXP2101_DCDC4_VOL2_MIN) / AXP2101_DCDC4_VOL_STEPS2;
        } else {
            return ESP_ERR_INVALID_ARG;
        }
        return axp2101_i2c_write_bytes(reg_addr, 1, &reg_val);

    case AXP2101_DCDC5:
        reg_addr = AXP2101_DC_VOL4_CTRL;
        if (voltage_mv == AXP2101_DCDC5_VOL_1200MV) {
            reg_val = AXP2101_DCDC5_VOL_VAL;
        } else if (voltage_mv >= AXP2101_DCDC5_VOL_MIN && voltage_mv <= AXP2101_DCDC5_VOL_MAX) {
            reg_val = (voltage_mv - AXP2101_DCDC5_VOL_MIN) / AXP2101_DCDC5_VOL_STEPS;
        } else {
            return ESP_ERR_INVALID_ARG;
        }
        ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(reg_addr, 1, &cur), TAG, "read DCDC5 vol failed");
        reg_val |= (cur & 0xE0);
        return axp2101_i2c_write_bytes(reg_addr, 1, &reg_val);

    default:
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t AXP2101_dcdc_set_pwm_mode(axp2101_dcdc_channel_t dcdc_ch, bool force_pwm)
{
    uint8_t bit;
    switch (dcdc_ch) {
    case AXP2101_DCDC1: bit = 2; break;
    case AXP2101_DCDC2: bit = 3; break;
    case AXP2101_DCDC3: bit = 4; break;
    case AXP2101_DCDC4: bit = 5; break;
    default: return ESP_ERR_INVALID_ARG;
    }

    uint8_t data;
    ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(AXP2101_DC_FORCE_PWM_CTRL, 1, &data),
                        TAG, "read DC_FORCE_PWM_CTRL failed");

    if (force_pwm) {
        data |= (1 << bit);
    } else {
        data &= ~(1 << bit);
    }

    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_DC_FORCE_PWM_CTRL, 1, &data),
                        TAG, "write DC_FORCE_PWM_CTRL failed");
    return ESP_OK;
}

esp_err_t AXP2101_dcdc_set_dvm(axp2101_dcdc_channel_t dcdc_ch, bool enable)
{
    uint8_t reg_addr;
    switch (dcdc_ch) {
    case AXP2101_DCDC2: reg_addr = AXP2101_DC_VOL1_CTRL; break;
    case AXP2101_DCDC3: reg_addr = AXP2101_DC_VOL2_CTRL; break;
    default: return ESP_ERR_INVALID_ARG;
    }

    uint8_t data;
    ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(reg_addr, 1, &data),
                        TAG, "read DCDC%d vol for DVM failed", dcdc_ch);

    if (enable) {
        data |= 0x80;
    } else {
        data &= ~0x80;
    }

    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(reg_addr, 1, &data),
                        TAG, "write DCDC%d DVM failed", dcdc_ch);
    return ESP_OK;
}

/* ===== LDO power rail control ===== */

esp_err_t AXP2101_ldo_enable(axp2101_ldo_channel_t ldo_ch, bool enable)
{
    uint8_t reg_addr;
    uint8_t bit;

    switch (ldo_ch) {
    case AXP2101_ALDO1:   reg_addr = AXP2101_LDO_ONOFF_CTRL0; bit = 0; break;
    case AXP2101_ALDO2:   reg_addr = AXP2101_LDO_ONOFF_CTRL0; bit = 1; break;
    case AXP2101_ALDO3:   reg_addr = AXP2101_LDO_ONOFF_CTRL0; bit = 2; break;
    case AXP2101_ALDO4:   reg_addr = AXP2101_LDO_ONOFF_CTRL0; bit = 3; break;
    case AXP2101_BLDO1:   reg_addr = AXP2101_LDO_ONOFF_CTRL0; bit = 4; break;
    case AXP2101_BLDO2:   reg_addr = AXP2101_LDO_ONOFF_CTRL0; bit = 5; break;
    case AXP2101_CPUSLDO: reg_addr = AXP2101_LDO_ONOFF_CTRL0; bit = 6; break;
    case AXP2101_DLDO1:   reg_addr = AXP2101_LDO_ONOFF_CTRL0; bit = 7; break;
    case AXP2101_DLDO2:   reg_addr = AXP2101_LDO_ONOFF_CTRL1; bit = 0; break;
    default: return ESP_ERR_INVALID_ARG;
    }

    uint8_t data;
    ESP_RETURN_ON_ERROR(axp2101_i2c_read_bytes(reg_addr, 1, &data),
                        TAG, "read LDO ONOFF failed");

    if (enable) {
        data |= (1 << bit);
    } else {
        data &= ~(1 << bit);
    }

    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(reg_addr, 1, &data),
                        TAG, "write LDO ONOFF failed");
    return ESP_OK;
}

esp_err_t AXP2101_ldo_set_voltage(axp2101_ldo_channel_t ldo_ch, uint16_t voltage_mv)
{
    uint8_t reg_addr;
    uint16_t v_min, v_max;
    uint16_t v_step;
    uint8_t  reg_val;

    switch (ldo_ch) {
    case AXP2101_ALDO1: reg_addr = AXP2101_ALDO1_V_CTRL; v_min = 500; v_max = 3500; v_step = 100; break;
    case AXP2101_ALDO2: reg_addr = AXP2101_ALDO2_V_CTRL; v_min = 500; v_max = 3500; v_step = 100; break;
    case AXP2101_ALDO3: reg_addr = AXP2101_ALDO3_V_CTRL; v_min = 500; v_max = 3500; v_step = 100; break;
    case AXP2101_ALDO4: reg_addr = AXP2101_ALDO4_V_CTRL; v_min = 500; v_max = 3500; v_step = 100; break;
    case AXP2101_BLDO1: reg_addr = AXP2101_BLDO1_V_CTRL; v_min = 500; v_max = 3500; v_step = 100; break;
    case AXP2101_BLDO2: reg_addr = AXP2101_BLDO2_V_CTRL; v_min = 500; v_max = 3500; v_step = 100; break;
    case AXP2101_CPUSLDO:
        reg_addr = AXP2101_CPUSLDO_V_CTRL; v_min = 500; v_max = 1400; v_step = 50; break;
    case AXP2101_DLDO1:
        reg_addr = AXP2101_DLDO1_V_CTRL; v_min = 500; v_max = 3300; v_step = 100; break;
    case AXP2101_DLDO2:
        reg_addr = AXP2101_DLDO2_V_CTRL; v_min = 500; v_max = 1400; v_step = 50; break;
    default: return ESP_ERR_INVALID_ARG;
    }

    if (voltage_mv < v_min || voltage_mv > v_max) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((voltage_mv - v_min) % v_step != 0) {
        ESP_LOGW(TAG, "LDO%d voltage %dmV not aligned to %dmV step, rounding down",
                 ldo_ch, voltage_mv, v_step);
    }

    reg_val = (voltage_mv - v_min) / v_step;
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(reg_addr, 1, &reg_val),
                        TAG, "write LDO%d voltage failed", ldo_ch);
    return ESP_OK;
}

//Status check item structure
typedef struct pmic_check_item{
    uint8_t byte_idx;
    uint8_t mask;
    const char *message;
    uint32_t event_flag;
    axp2101_event_handler_t handler;
} pmic_check_item_t;

//IRQ Status Checklist
static pmic_check_item_t axp2101_IRQstatus_checks[] = {
    // status of byte0 — IRQ_STATUS1 (REG48H)
    {0, 0x80, "SOC drop to Warning level",         AXP2101_EVENT_SOC_WARNING,    NULL},
    {0, 0x40, "SOC drop to Shutdown level",        AXP2101_EVENT_SOC_SHUTDOWN,   NULL},
    {0, 0x20, "Gague Watchdog Timeout IRQ",        AXP2101_EVENT_GAUGE_WDT_TIMEOUT, NULL},
    {0, 0x10, "Gague New SOC IRQ",                 AXP2101_EVENT_GAUGE_NEW_SOC,  NULL},
    {0, 0x0A, "Battery Over Temperature",          AXP2101_EVENT_BAT_OVER_TEMP,  NULL},
    {0, 0x05, "Battery Under Temperature",         AXP2101_EVENT_BAT_UNDER_TEMP, NULL},

    // status of byte1 — IRQ_STATUS2 (REG49H)
    {1, 0x80, "VBUS Insert",                       AXP2101_EVENT_VBUS_INSERT,    NULL},
    {1, 0x40, "VBUS Remove",                       AXP2101_EVENT_VBUS_REMOVE,    NULL},
    {1, 0x20, "Battery Insert",                    AXP2101_EVENT_BAT_INSERT,     NULL},
    {1, 0x10, "Battery Remove",                    AXP2101_EVENT_BAT_REMOVE,     NULL},
    {1, 0x0F, "POWERON Press",                     AXP2101_EVENT_POWERON_PRESS,  NULL},

    // status of byte2 — IRQ_STATUS3 (REG4AH)
    {2, 0x80, "Watchdog Expire",                   AXP2101_EVENT_WDT_EXPIRE,     NULL},
    {2, 0x40, "LDO Over Current",                  AXP2101_EVENT_LDO_OC,         NULL},
    {2, 0x20, "BATFET Over Current Protection IRQ", AXP2101_EVENT_BATFET_OCP,     NULL},
    {2, 0x10, "Battary Charge done",               AXP2101_EVENT_CHARGE_DONE,    NULL},
    {2, 0x08, "Battery Charge start",              AXP2101_EVENT_CHARGE_START,   NULL},
    {2, 0x04, "DIE Over Temperature level1 IRQ",   AXP2101_EVENT_DIE_OVERTEMP,   NULL},
    {2, 0x02, "Charger Safety Timer1/2 expire",    AXP2101_EVENT_CHG_TIMER_EXPIRE, NULL},
    {2, 0x01, "Battery Over Voltage Protection",   AXP2101_EVENT_BAT_OVP,        NULL},

    // status of byte3 — STATUS1 (REG00H)
    {3, 0x20, "status   VBUS GOOD",                AXP2101_STATUS_VBUS_GOOD,     NULL},
    {3, 0x10, "status   BATFET open",              AXP2101_STATUS_BATFET_OPEN,   NULL},
    {3, 0x08, "status   Battery Present",          AXP2101_STATUS_BAT_PRESENT,   NULL},
    {3, 0x04, "status   Battery in Active Mode",   AXP2101_STATUS_BAT_ACTIVE,    NULL},
    {3, 0x02, "status   In Thermal Regulation",    AXP2101_STATUS_THERMAL_REG,   NULL},
    {3, 0x01, "status   In Current Limit State",   AXP2101_STATUS_CURRENT_LIMIT, NULL},

    // status of byte4 — STATUS2 (REG01H)
    {4, 0x10, "status   System is Power ON",       AXP2101_STATUS_SYS_POWERON,   NULL},
    {4, 0x08, "status   In VINDPM",                AXP2101_STATUS_VINDPM,        NULL},

    {0xFF, 0, NULL, 0, NULL}
};

esp_err_t AXP2101_register_handler(uint32_t event_flag, axp2101_event_handler_t handler)
{
    if (!handler || !event_flag) return ESP_ERR_INVALID_ARG;
    for (int i = 0; axp2101_IRQstatus_checks[i].message != NULL; i++) {
        if (axp2101_IRQstatus_checks[i].event_flag == event_flag) {
            axp2101_IRQstatus_checks[i].handler = handler;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t AXP2101_unregister_handler(uint32_t event_flag)
{
    for (int i = 0; axp2101_IRQstatus_checks[i].message != NULL; i++) {
        if (axp2101_IRQstatus_checks[i].event_flag == event_flag) {
            axp2101_IRQstatus_checks[i].handler = NULL;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

static esp_err_t AXP2101_IRQStatus_clear(void)
{
    static const uint8_t data[3] = {0xFF, 0xFF, 0xFF};
    ESP_RETURN_ON_ERROR(axp2101_i2c_write_bytes(AXP2101_IRQ_STATUS1, sizeof(data), data),
                        TAG, "clear IRQ status failed");
    return ESP_OK;
}

static uint32_t AXP2101_check_status(void)
{
    uint32_t flags = 0;
    uint8_t status_data[AXP2101_IRQ_STATUS_CNT + 2];

    if (axp2101_i2c_read_bytes(AXP2101_IRQ_STATUS1, AXP2101_IRQ_STATUS_CNT, status_data) != ESP_OK) {
        return 0;
    }
    if (axp2101_i2c_read_bytes(AXP2101_STATUS1, 2, &status_data[AXP2101_IRQ_STATUS_CNT]) != ESP_OK) {
        return 0;
    }
    AXP2101_IRQStatus_clear();

    pmic_check_item_t *item = axp2101_IRQstatus_checks;
    while (item->message != NULL) {
        bool triggered = false;
        if ((item->mask & (item->mask - 1)) != 0) {
            if (status_data[item->byte_idx] & item->mask) {
                triggered = true;
            }
        } else {
            if (status_data[item->byte_idx] & item->mask) {
                triggered = true;
            }
        }

        if (triggered) {
            AMOLED_console_log(INFORM, false, TAG, item->message);
            if (item->event_flag) {
                flags |= item->event_flag;
            }
            if (item->handler) {
                item->handler(item->event_flag);
            }
        }
        item++;
    }
    return flags;
}


