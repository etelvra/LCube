#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_check.h"
#include "driver/gpio.h"

#include "Pin_Definitions.h"
#include "AXP2101Constants.h"
#include "AXP2101.h"
#include "light_sleep.h"
#include "panel.h"

static const char* TAG = "PMIC";

bool i2c_bus_initialized = false;
QueueHandle_t pmic_event_queue = NULL;
i2c_bus_handle_t i2c_bus_handle = NULL;
static i2c_bus_device_handle_t axp2101_i2c_device_handle = NULL;
static TimerHandle_t pmic_timer = NULL;

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

static void PMIC_timer_callback(TimerHandle_t xTimer)
{
    uint32_t trigger = 1;
    xQueueSend(pmic_event_queue, &trigger, 0);
}

static void task_pmic_management(void *param);
static esp_err_t AXP2101_check_status(axp2101_status_t *axp2101_status);
static uint16_t PMIC_status_list_refresh(const axp2101_status_t *axp2101_status);
static void PMIC_irq_log_refresh(uint16_t y_star, const axp2101_status_t *axp2101_status);

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

    //timer for PMIC status refresh
    pmic_timer = xTimerCreate("pmic_tmr", pdMS_TO_TICKS(3000),
                                        pdTRUE, NULL, PMIC_timer_callback);
    if (pmic_timer)  xTimerStart(pmic_timer, 0);

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
    AXP2101_ldo_enable(AXP2101_ALDO1, 0);
    AXP2101_ldo_enable(AXP2101_BLDO1, 0);
    AXP2101_ldo_enable(AXP2101_CPUSLDO, 0);

    axp2101_status_t pmic_status;
    AXP2101_check_status(&pmic_status); /* clear any pending IRQ before entering main loop */

    uint32_t io_num;
    bool pmic_monitor = false;
    uint16_t y_pos;

    while (1) {
        if (xQueueReceive(pmic_event_queue, &io_num, portMAX_DELAY)) {
            if (io_num == 0xFFFFFFFF){
                xTimerChangePeriod(pmic_timer, pdMS_TO_TICKS(1000), 0);
                AMOLED_refresh();
                pmic_monitor = true;
            } else if (!io_num){
                pmic_monitor = false;
                xTimerChangePeriod(pmic_timer, pdMS_TO_TICKS(10000), 0);
            }
            AXP2101_check_status(&pmic_status);
            if (pmic_status.battery_pct > 0 && pmic_status.battery_pct <= 10) {
                AXP2101_sys_shutdown();
            } else if (pmic_status.battery_pct > 0 && pmic_status.battery_pct <= 20
                       && !(pmic_status.pmu_status_flags & AXP2101_PMU_STS_VBUS_GOOD)) {
                AXP2101_sys_shutdown();
            }
            if (pmic_monitor){
                y_pos = PMIC_status_list_refresh(&pmic_status);
                if (io_num == IOPIN_PMIC_IRQ ) PMIC_irq_log_refresh(y_pos, &pmic_status);
            }
            AMOLED_console_log(INFORM, false, TAG, "bat percentage is %d\n", pmic_status.battery_pct);
            if (io_num == IOPIN_PMIC_IRQ ) AMOLED_console_log(WARN, false, TAG, "AXP2101 IRQ triggered\n");
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


static esp_err_t AXP2101_check_status(axp2101_status_t *axp2101_status)
{
    if (!axp2101_status) return ESP_ERR_INVALID_ARG;
    memset(axp2101_status, 0, sizeof(*axp2101_status));

    esp_err_t ret, first_err = ESP_OK;
    uint8_t  buf[11];
    uint16_t raw;
    uint8_t  v;

    /*Ensure ADC channels are enabled (REG30 bits 5:0)*/
    buf[0] = 0x3F;
    axp2101_i2c_write_bytes(AXP2101_ADC_CHANNEL_CTRL, 1, buf);

    /* STATUS1 + STATUS2 → pmu_status_flags (REG00~01, 2 B)*/
    ret = axp2101_i2c_read_bytes(AXP2101_STATUS1, 2, buf);
    if (ret == ESP_OK) {
        axp2101_status->pmu_status_flags = buf[0] | ((uint16_t)buf[1] << 8);
    } else if (first_err == ESP_OK) { first_err = ret; }

    /* Die OTP threshold (REG13[2:1] → 115/125/135 °C)*/
    ret = axp2101_i2c_read_bytes(AXP2101_DIE_TEMP_CTRL, 1, buf);
    if (ret == ESP_OK) {
        static const uint8_t otp_map[] = {115, 125, 135, 0};
        axp2101_status->die_otp_threshold = otp_map[(buf[0] >> 1) & 3];
    } else if (first_err == ESP_OK) { first_err = ret; }

    /* PWRON / PWROFF source (REG20~21, 2 B)*/
    ret = axp2101_i2c_read_bytes(AXP2101_PWRON_STATUS, 2, buf);
    if (ret == ESP_OK) {
        axp2101_status->pwr_on_source  = buf[0];
        axp2101_status->pwr_off_source = buf[1];
    } else if (first_err == ESP_OK) { first_err = ret; }

    /* ADC data (REG34~3D, 10 B consecutive)*/
    ret = axp2101_i2c_read_bytes(AXP2101_ADC_DATA_RELUST0, 10, buf);
    if (ret == ESP_OK) {
        /* VBAT: buf[0]=REG34_h, buf[1]=REG35_l → 14-bit, 0.5 mV/LSB */
        raw = ((buf[0] & 0x3F) << 8) | buf[1];
        axp2101_status->vbat_mv = raw;

        /* TS pin raw ADC: buf[2]=REG36_h, buf[3]=REG37_l */
        raw = ((buf[2] & 0x3F) << 8) | buf[3];
        axp2101_status->ts_mv = raw / 2;

        /* VBUS: buf[4]=REG38_h, buf[5]=REG39_l */
        raw = ((buf[4] & 0x3F) << 8) | buf[5];
        axp2101_status->vbus_mv = raw;

        /* VSYS: buf[6]=REG3A_h, buf[7]=REG3B_l */
        raw = ((buf[6] & 0x3F) << 8) | buf[7];
        axp2101_status->vsys_mv = raw ;

        /* Die temp: buf[8]=REG3C_h, buf[9]=REG3D_l → 0.1 °C units */
        raw = ((buf[8] & 0x3F) << 8) | buf[9];
        axp2101_status->die_temp = 220 + (7274 - (int32_t)raw) / 2;
    } else if (first_err == ESP_OK) { first_err = ret; }

    /* IRQ Status (REG48~4A, 3 B) → then clear*/
    ret = axp2101_i2c_read_bytes(AXP2101_IRQ_STATUS1, AXP2101_IRQ_STATUS_CNT, buf);
    if (ret == ESP_OK) {
        axp2101_status->irq_status_flags = buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16);

        /* Write-1-to-clear all IRQ latches */
        static const uint8_t clear[3] = {0xFF, 0xFF, 0xFF};
        axp2101_i2c_write_bytes(AXP2101_IRQ_STATUS1, 3, clear);
    } else if (first_err == ESP_OK) { first_err = ret; }

    /* Charger config (REG61~64, 4 B) ──
     * Stored as raw register nibbles since decoded mA/mV overflow uint8_t
     * for ichg (max 1000 mA) and cv_voltage (max 4400 mV). */
    ret = axp2101_i2c_read_bytes(AXP2101_IPRECHG_SET, 4, buf);
    if (ret == ESP_OK) {
        /* NOTE: iprechg_ma / iterm_ma hold decoded mA (0~200 fits uint8_t).
         *        ichg_ma / cv_voltage_mv hold raw register values because
         *        decoded values overflow uint8_t.  */
        v = buf[0] & 0x0F;                         /* REG61[3:0] */
        axp2101_status->iprechg_ma = (v <= 8) ? v * 25 : 0;
        v = buf[1] & 0x1F;                         /* REG62[4:0] */
        axp2101_status->ichg_ma0 = (v <= 8) ? v * 2.5 : 20 + (v-8)*10;
        v = buf[2] & 0x0F;                         /* REG63[3:0] */
        axp2101_status->iterm_ma = (v <= 8) ? v * 25 : 0;
        v = buf[3] & 0x07;                         /* REG64[2:0] */
        axp2101_status->cv_voltage_mv00 = 39 + v;
    } else if (first_err == ESP_OK) { first_err = ret; }

    /* DCDC on/off + voltage (REG80~86, 7 B)*/
    ret = axp2101_i2c_read_bytes(AXP2101_DC_ONOFF_DVM_CTRL, 7, buf);
    if (ret == ESP_OK) {
        /* buf[0]=REG80: DCDC1~5 enable bits[4:0] */
        for (int i = 0; i < 5; i++) {
            axp2101_status->dcdc[i].enabled = (buf[0] >> i) & 1;
        }

        /* DCDC1: buf[2]=REG82[4:0], 1500~3400 mV, 100 mV/step */
        axp2101_status->dcdc[0].voltage_mv = 1500 + (buf[2] & 0x1F) * 100;

        /* DCDC2: buf[3]=REG83[6:0] (bit7=DVM, not voltage) */
        v = buf[3] & 0x7F;
        if      (v <= 70)  axp2101_status->dcdc[1].voltage_mv = 500  + v * 10;
        else if (v <= 87)  axp2101_status->dcdc[1].voltage_mv = 1220 + (v - 71) * 20;

        /* DCDC3: buf[4]=REG84[6:0] */
        v = buf[4] & 0x7F;
        if      (v <= 70)  axp2101_status->dcdc[2].voltage_mv = 500  + v * 10;
        else if (v <= 87)  axp2101_status->dcdc[2].voltage_mv = 1220 + (v - 71) * 20;
        else if (v <= 107) axp2101_status->dcdc[2].voltage_mv = 1600 + (v - 88) * 100;

        /* DCDC4: buf[5]=REG85[6:0] */
        v = buf[5] & 0x7F;
        if      (v <= 70)  axp2101_status->dcdc[3].voltage_mv = 500  + v * 10;
        else if (v <= 102) axp2101_status->dcdc[3].voltage_mv = 1220 + (v - 71) * 20;

        /* DCDC5: buf[6]=REG86[4:0], 1400~3700mV(100mV/step) or 0x19=1200mV */
        v = buf[6] & 0x1F;
        axp2101_status->dcdc[4].voltage_mv = (v == 0x19) ? 1200 : 1400 + v * 100;
    } else if (first_err == ESP_OK) { first_err = ret; }

    /* LDO on/off + voltage (REG90~9A, 11 B)*/
    ret = axp2101_i2c_read_bytes(AXP2101_LDO_ONOFF_CTRL0, 11, buf);
    if (ret == ESP_OK) {
        /* buf[0]=REG90: ALDO1~4, BLDO1~2, CPUSLDO, DLDO1 enable [7:0] */
        for (int i = 0; i < 8; i++) {
            axp2101_status->ldo[i].enabled = (buf[0] >> i) & 1;
        }
        /* buf[1]=REG91[0]: DLDO2 enable */
        axp2101_status->ldo[8].enabled = buf[1] & 1;

        /* ALDO1~4 BLDO1~2 voltages: buf[2]~buf[7] = REG92~97,  500~3500mV, 100mV/step */
        for (int i = 0; i < 6; i++) {
            axp2101_status->ldo[i].voltage_mv = 500 + (buf[2 + i] & 0x1F) * 100;
        }

        /* CPUSLDO: buf[8]=REG98, 500~1400mV, 50mV/step */
        axp2101_status->ldo[6].voltage_mv = 500 + (buf[8] & 0x1F) * 50;
        /* DLDO1:   buf[9]=REG99, 500~3300mV, 100mV/step */
        axp2101_status->ldo[7].voltage_mv = 500 + (buf[9] & 0x1F) * 100;
        /* DLDO2:   buf[10]=REG9A, 500~1400mV, 50mV/step */
        axp2101_status->ldo[8].voltage_mv = 500 + (buf[10] & 0x1F) * 50;
    } else if (first_err == ESP_OK) { first_err = ret; }

    /* Battery percentage (REGA4)*/
    ret = axp2101_i2c_read_bytes(AXP2101_BATTERY_PERCENTAGE, 1, &axp2101_status->battery_pct);
    if (ret != ESP_OK && first_err == ESP_OK) { first_err = ret; }

    return first_err;
}

#define pmu_s(flag) (axp2101_status->pmu_status_flags & (flag) ? "YES" : "NO")

static char *rail_fmt(const axp2101_power_rail_t *r, char *buf, size_t sz)
{
    if (r->enabled) {
        snprintf(buf, sz, "%umV", r->voltage_mv);
        return buf;
    }
    return "[OFF]";
}

static const char *chg_stat_str(uint16_t pmu_status_flags)
{
    switch (pmu_status_flags & AXP2101_PMU_STS_CHG_STAT_MASK) {
    case AXP2101_PMU_STS_CHG_STAT_TRI:     return "Trickle chg";
    case AXP2101_PMU_STS_CHG_STAT_PRE:     return "Pre-charge";
    case AXP2101_PMU_STS_CHG_STAT_CC:      return "Const current";
    case AXP2101_PMU_STS_CHG_STAT_CV:      return "Const voltage";
    case AXP2101_PMU_STS_CHG_STAT_DONE:    return "Charge done";
    case AXP2101_PMU_STS_CHG_STAT_NOT_CHG: return "Not charging";
    default:                               return "---";
    }
}

static const char *pwr_on_src_str(uint8_t src)
{
    if (src & (1 << 5)) return "EN High";   /* EN pin always high */
    if (src & (1 << 4)) return "Bat Ins";   /* Battery insert & good */
    if (src & (1 << 3)) return "Bat>3.3";  /* VBAT > 3.3V while charging */
    if (src & (1 << 2)) return "VBUS In";   /* VBUS insert & good */
    if (src & (1 << 1)) return "IRQ Low";   /* IRQ pin pull-down */
    if (src & (1 << 0)) return "POK on";       /* PWRON key on-level */
    return "---";
}

static const char *pwr_off_src_str(uint8_t src)
{
    if (src & (1 << 7)) return "Die OTP";   /* Die over-temp level2 */
    if (src & (1 << 6)) return "DCDC OV";   /* DCDC over-voltage */
    if (src & (1 << 5)) return "DCDC UV";   /* DCDC under-voltage */
    if (src & (1 << 4)) return "VBUS OV";   /* VBUS over-voltage */
    if (src & (1 << 3)) return "VSYS UV";   /* VSYS under-voltage */
    if (src & (1 << 2)) return "EN Low";    /* EN pin always low */
    if (src & (1 << 1)) return "Soft";      /* Software shutdown */
    if (src & (1 << 0)) return "POK off";       /* PWRON key off-level */
    return "---";
}

static const char *bat_dir_str(uint16_t pmu_status_flags)
{
    if (!(pmu_status_flags & AXP2101_PMU_STS_BAT_PRESENT)) {
        return "Absent";
    }
    switch (pmu_status_flags & AXP2101_PMU_STS_BAT_DIR_MASK) {
    case AXP2101_PMU_STS_BAT_DIR_STANDBY:   return "Standby";
    case AXP2101_PMU_STS_BAT_DIR_CHARGE:    return "Charge";
    case AXP2101_PMU_STS_BAT_DIR_DISCHARGE: return "Discharge";
    default:                                return "---";
    }
}

static uint16_t PMIC_status_list_refresh(const axp2101_status_t *axp2101_status)
{
    esp_lcd_panel_swap_xy(amoled_panel_handle, 0);
    esp_lcd_panel_mirror(amoled_panel_handle, 1, 1);

    uint16_t y = axp2101_status->vbat_mv%5;
    AMOLED_print_single_line(0, y, true, "                 PMIC: AXP2101                 "); y += 16;
    AMOLED_print_single_line(0, y, true, "==============================================="); y += 16;
    AMOLED_print_single_line(0, y, true, "      VBUS: %4umV        VSYS: %4umV           ", axp2101_status->vbus_mv, axp2101_status->vsys_mv); y += 16;
    AMOLED_print_single_line(0, y, true, "      SYSTEM: %s                               ", axp2101_status->pmu_status_flags&AXP2101_PMU_STS_SYS_POWERON ? "powerON" : "powerOFF"); y += 16;
    AMOLED_print_single_line(0, y, true, "      PWRON: %-8s     PWROFF: %-8s             ", pwr_on_src_str(axp2101_status->pwr_on_source), pwr_off_src_str(axp2101_status->pwr_off_source)); y += 16;
    AMOLED_print_single_line(0, y, true, "------------------- Battery -------------------"); y += 16;
    AMOLED_print_single_line(0, y, true, "     Battery: %-9s   %3u%%  %4umV /%d.%dV      ",
        bat_dir_str(axp2101_status->pmu_status_flags), axp2101_status->battery_pct, axp2101_status->vbat_mv, axp2101_status->cv_voltage_mv00 / 10, axp2101_status->cv_voltage_mv00 % 10); y += 16;
    AMOLED_print_single_line(0, y, true, "     Active Mode: %-3s     BATFET open: %-3s   ", pmu_s(AXP2101_PMU_STS_BAT_ACTIVE), pmu_s(AXP2101_PMU_STS_BATFET_OPEN)); y += 16;
    AMOLED_print_single_line(0, y, true, "------------------ Charging -------------------"); y += 16;
    AMOLED_print_single_line(0, y, true, "     VBUS: %-7s    %-14s          ", axp2101_status->pmu_status_flags & AXP2101_PMU_STS_VBUS_GOOD ? "GOOD" : "Removed", chg_stat_str(axp2101_status->pmu_status_flags)); y += 16;
    AMOLED_print_single_line(0, y, true, "     IINDPM:  %-3s       VINDPM:  %-3s   ", pmu_s(AXP2101_PMU_STS_CURRENT_LIMIT), pmu_s(AXP2101_PMU_STS_VINDPM)); y += 16;
    AMOLED_print_single_line(0, y, true, "    IPRECHG: %3umA  ICHG: %3u0mA  ITERM: %3umA ", axp2101_status->iprechg_ma, axp2101_status->ichg_ma0, axp2101_status->iterm_ma); y += 16;
    AMOLED_print_single_line(0, y, true, "----------------- Temperature -----------------"); y += 16;
    AMOLED_print_single_line(0, y, true, "     Die: %4u / %3u'C   TS(BAT): %4umV         ", axp2101_status->die_temp, axp2101_status->die_otp_threshold, axp2101_status->ts_mv); y += 16;
    AMOLED_print_single_line(0, y, true, "     Thermal Regulation:     %-3s              ", pmu_s(AXP2101_PMU_STS_THERMAL_REG)); y += 16;
    AMOLED_print_single_line(0, y, true, "----------------- Power Rails -----------------"); y += 16;
    {
    char rb[3][8];
    AMOLED_print_single_line(0, y, true, "     DCDC1 %-6s  DCDC2 %-6s  DCDC3 %-6s  ",
        rail_fmt(&axp2101_status->dcdc[0], rb[0], sizeof(rb[0])),
        rail_fmt(&axp2101_status->dcdc[1], rb[1], sizeof(rb[1])),
        rail_fmt(&axp2101_status->dcdc[2], rb[2], sizeof(rb[2]))); y += 16;
    AMOLED_print_single_line(0, y, true, "     DCDC4 %-6s  DCDC5 %-6s              ",
        rail_fmt(&axp2101_status->dcdc[3], rb[0], sizeof(rb[0])),
        rail_fmt(&axp2101_status->dcdc[4], rb[1], sizeof(rb[1]))); y += 16;
    AMOLED_print_single_line(0, y, true, "     ALDO1 %-6s  ALDO2 %-6s  ALDO3 %-6s  ",
        rail_fmt(&axp2101_status->ldo[0], rb[0], sizeof(rb[0])),
        rail_fmt(&axp2101_status->ldo[1], rb[1], sizeof(rb[1])),
        rail_fmt(&axp2101_status->ldo[2], rb[2], sizeof(rb[2]))); y += 16;
    AMOLED_print_single_line(0, y, true, "     ALDO4 %-6s  BLDO1 %-6s  BLDO2 %-6s  ",
        rail_fmt(&axp2101_status->ldo[3], rb[0], sizeof(rb[0])),
        rail_fmt(&axp2101_status->ldo[4], rb[1], sizeof(rb[1])),
        rail_fmt(&axp2101_status->ldo[5], rb[2], sizeof(rb[2]))); y += 16;
    AMOLED_print_single_line(0, y, true, "    CPUSLD %-6s  DLDO1 %-6s  DLDO2 %-6s  ",
        rail_fmt(&axp2101_status->ldo[6], rb[0], sizeof(rb[0])),
        rail_fmt(&axp2101_status->ldo[7], rb[1], sizeof(rb[1])),
        rail_fmt(&axp2101_status->ldo[8], rb[2], sizeof(rb[2]))); y += 16;
    }
    AMOLED_print_single_line(0, y, true, "------------------ IRQ logs -------------------"); y += 16;

    esp_lcd_panel_swap_xy(amoled_panel_handle, 1);
    esp_lcd_panel_mirror(amoled_panel_handle, 1, 0);
    return y;
}

static void PMIC_irq_log_refresh(uint16_t y_star, const axp2101_status_t *axp2101_status)
{
    esp_lcd_panel_swap_xy(amoled_panel_handle, 0);
    esp_lcd_panel_mirror(amoled_panel_handle, 1, 1);
    uint8_t irq_log_num = 0;
    uint16_t y = y_star;
    for(int irq=0; irq<32; irq++) {
        if (axp2101_status->irq_status_flags & (1UL << irq)) {
            AMOLED_print_single_line(0, y, true, "     %-48s", axp2101_irq_status_strings[irq]); y += 16;
            irq_log_num++;
        if (irq_log_num > 6) break;
      }
    }
    while (y<448) {
        AMOLED_print_single_line(0, y, true, "                                               "); y += 16;
    }
    esp_lcd_panel_swap_xy(amoled_panel_handle, 1);
    esp_lcd_panel_mirror(amoled_panel_handle, 1, 0);
}
