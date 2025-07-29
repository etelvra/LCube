#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_check.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "IO_PIN_NUM.h"
#include "AXP2101Constants.h"
#include "AXP2101.h"

i2c_master_bus_handle_t i2c_handle = NULL;
bool i2c_initialized = false;
i2c_master_dev_handle_t axp2101_hd = NULL;

//Create the interrupt service function
QueueHandle_t axp2101_evt_queue = NULL;
static void IRAM_ATTR PMIC_IRQfunction_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(axp2101_evt_queue, &gpio_num, NULL);
}

static void task_power_management(void *param)
{
    uint32_t io_num;
    uint8_t address = AXP2101_IRQ_STATUS1;
    uint8_t data[AXP2101_IRQ_STATUS_CNT];
    while (1){ if (xQueueReceive(axp2101_evt_queue, &io_num,portMAX_DELAY)) {//wait for interrupt
        if (io_num==IOPIN_PMIC_PWR){
            i2c_master_transmit_receive(axp2101_hd, &address, 1,data, sizeof(data),1000);
            check_axp2101_status(data);//数组名在传递给函数时会退化为指针
            ESP_LOGI("task_power_management","AXP2101_IRQsta is %x  %X  %X",data[0],data[1],data[2]);
            AXP2101_IRQStatus_clear();
            }
            printf("GPIO[%"PRIu32"] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
    vTaskDelete(NULL);
}

void AXP2101_init(void)
{
    i2c_init();
    uint8_t data[6] = {AXP2101_DC_ONOFF_DVM_CTRL,0x01,AXP2101_LDO_ONOFF_CTRL0,0x00, AXP2101_LDO_ONOFF_CTRL1,0x00};
    uint8_t address=0x00;
    const i2c_device_config_t axp2101_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_ADDRESS,
        .scl_speed_hz = 200000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle, &axp2101_config, &axp2101_hd));
    i2c_master_transmit(axp2101_hd, &data[0], 2,100);
    i2c_master_transmit(axp2101_hd, &data[2], 4,100);
    i2c_master_transmit_receive(axp2101_hd, &address, 1,data, sizeof(data),500);
    //configure GPIO for axp2101 interrupt
    const gpio_config_t PMIC_irq_config = {
        .pin_bit_mask = (1<<IOPIN_PMIC_PWR),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .mode         = GPIO_MODE_INPUT,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&PMIC_irq_config);
    gpio_set_intr_type(IOPIN_PMIC_PWR, GPIO_INTR_NEGEDGE);
    axp2101_evt_queue = xQueueCreate(10, sizeof(uint32_t));             //create a queue to handle gpio event from isr
    xTaskCreate(task_power_management,"task_pwr_management",8192,NULL,5,NULL);//数字越小越优先
    gpio_install_isr_service(ESP_INTR_FLAG_LOWMED|ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_EDGE);//install gpio isr service(high priority)
//    gpio_isr_handler_add(IOPIN_PMIC_IRQ, PMIC_IRQfunction_handler, (void*) IOPIN_PMIC_IRQ);        //hook isr handler for specific gpio pin
    gpio_isr_handler_add(IOPIN_PMIC_PWR, PMIC_IRQfunction_handler, (void*) IOPIN_PMIC_PWR);
    ESP_LOGI("task_AXP2101_INIT","AXP2101_status is %x   %x",data[0],data[1]);
}

esp_err_t AXP2101_bat_percentage(void)
{
    uint8_t data=AXP2101_BATTERY_PERCENTAGE;
    i2c_master_transmit_receive(axp2101_hd, &data, 1, &data, sizeof(data),100);
    return data;
}

void AXP2101_IRQStatus_clear(void)
{
    const uint8_t data[4]={AXP2101_IRQ_STATUS1,0xff,0xff,0xff};
    i2c_master_transmit(axp2101_hd, &data[0], sizeof(data),100);
}

esp_err_t i2c_init(void)
{
    /*IF I2C was initialized before */
    if (!i2c_initialized) {
    const i2c_master_bus_config_t i2c_config = {
        .i2c_port   = I2C_NUM_0,
        .sda_io_num = IOPIN_I2C_SDA,
        .scl_io_num = IOPIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .trans_queue_depth = 10,
        };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &i2c_handle));
    i2c_initialized = true;
    }
    return ESP_OK;
}

esp_err_t i2c_deinit(void)
{
    ESP_ERROR_CHECK(i2c_del_master_bus(i2c_handle));
    i2c_initialized = false;
    return ESP_OK;
}

//Status check item structure
typedef struct {
    uint8_t byte_idx;      // Data byte index (0,1,2
    uint8_t mask;          // bitmask
    const char *message;   // state description
} StatusCheckItem;

//Status Checklist - Centrally manage all status detection logics
static const StatusCheckItem axp2101_status_checks[] = {
    // status of byte0
    {0, 0x80, "SOC drop to Warning level"},
    {0, 0x40, "SOC drop to Shutdown level"},
    {0, 0x20, "Gague Watchdog Timeout IRQ"},
    {0, 0x10, "Gague New SOC IRQ"},
    // 温度状态使用组合检查
    {0, 0x0A, "Battery Over Temperature"},
    {0, 0x05, "Battery Under Temperature"},

    // status of byte1
    {1, 0x80, "VBUS Insert"},
    {1, 0x40, "VBUS Remove"},
    {1, 0x20, "Battery Insert"},
    {1, 0x10, "Battery Remove"},
    {1, 0x0F, "POWERON Press"},

    // status of byte2
    {2, 0x80, "Watchdog Expire"},
    {2, 0x40, "LDO Over Current"},
    {2, 0x20, "BATFET Over Current Protuction IRQ"},
    {2, 0x10, "Battary Charge done"},
    {2, 0x08, "Battery Charge start"},
    {2, 0x04, "DIE Over Temperature level1 IRQ"},
    {2, 0x02, "Charger Safety Timer1/2 expire"},
    {2, 0x01, "Battery Over Voltage Protection"},
    // end mark
    {0xFF, 0, NULL}
};

static void print_power_management(const char* power_status)
{
    ESP_LOGI("AXP2101_PWR","status %s",power_status);
}

void check_axp2101_status(uint8_t *status_data)
{
    StatusCheckItem *item = axp2101_status_checks;
    while (item->message != NULL) {
        // Check the multi-bit status
        if ((item->mask & (item->mask - 1)) != 0) {  // Determine whether it is a multi-bit mask
            if ((status_data[item->byte_idx] & item->mask)) {//完全符合取if ((status_data[item->byte_idx] & item->mask) == item->mask) {
                print_power_management(item->message);
            }
        }
        // Check the single-bit status
        else {
            if (status_data[item->byte_idx] & item->mask) {
                print_power_management(item->message);
            }
        }
        item++;
    }
}


