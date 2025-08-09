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
bool i2c_initialized = false;
i2c_bus_handle_t i2c_bus_handle = NULL;
i2c_bus_device_handle_t axp2101_i2c_device_handle = NULL;

static esp_err_t axp2101_i2c_read_bytes(uint8_t mem_address, size_t data_len, uint8_t *data) {
    return i2c_bus_read_bytes(axp2101_i2c_device_handle, mem_address, data_len, data);
}

static esp_err_t axp2101_i2c_write_bytes(uint8_t mem_address, size_t data_len, const uint8_t *data) {
    return i2c_bus_write_bytes(axp2101_i2c_device_handle, mem_address, data_len, data);
}

static void AXP2101_print(const char* power_status)
{
    ESP_LOGI("AXP2101_PWR","%s",power_status);
}


//Create the interrupt service function
QueueHandle_t axp2101_evt_queue = NULL;
static void IRAM_ATTR PMIC_IRQfunction_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(axp2101_evt_queue, &gpio_num, NULL);
}
static void task_power_management(void *param);
static void check_axp2101_status(uint8_t *status_data);

void AXP2101_init(void)
{
    i2c_init();
    axp2101_i2c_device_handle = i2c_bus_device_create(i2c_bus_handle, AXP2101_ADDRESS, 400000);

    uint8_t data[3] = {0x01, 0x04,0x00};
    axp2101_i2c_write_bytes(AXP2101_DC_ONOFF_DVM_CTRL, 1 , &data[0]);
    axp2101_i2c_write_bytes(AXP2101_LDO_ONOFF_CTRL0, 2 , &data[1]);
    axp2101_i2c_read_bytes(AXP2101_STATUS1, 2 , data);

    //configure GPIO for axp2101 interrupt
    const gpio_config_t PMIC_irq_config = {
        .pin_bit_mask = (1ULL<<IOPIN_PMIC_PWR),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .mode         = GPIO_MODE_INPUT,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&PMIC_irq_config);
    gpio_set_intr_type(IOPIN_PMIC_PWR, GPIO_INTR_NEGEDGE);
    //create a queue to handle gpio event from isr
    axp2101_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(task_power_management,"task_pwr_management",8192,NULL,5,NULL);//数字越小越优先
    //Configure only once to install gpio isr service(high priority)for the project
    gpio_install_isr_service(ESP_INTR_FLAG_LOWMED|ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_EDGE);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(IOPIN_PMIC_PWR, PMIC_IRQfunction_handler, (void*) IOPIN_PMIC_PWR);
//    gpio_isr_handler_add(IOPIN_PMIC_IRQ, PMIC_IRQfunction_handler, (void*) IOPIN_PMIC_IRQ);

    ESP_LOGI("task_AXP2101_INIT","AXP2101_status is %x   %x",data[0],data[1]);

//    gpio_isr_handler_remove(IOPIN_PMIC_PWR);
//    gpio_uninstall_isr_service();
}

esp_err_t i2c_init(void)
{
    /*IF I2C was initialized before */
    if (!i2c_initialized) {
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
/*    const i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port   = I2C_BUS_PORT,
        .sda_io_num = IOPIN_I2C_SDA,
        .scl_io_num = IOPIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .trans_queue_depth = 10,
        };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle)); */
    i2c_initialized = true;
    }
    return ESP_OK;
}

static void AXP2101_IRQStatus_clear(void)
{
    const uint8_t data[3]={0xFF,0xFF,0xFF};
    axp2101_i2c_write_bytes(AXP2101_IRQ_STATUS1,sizeof(data),data);
}

static void task_power_management(void *param)
{
    uint32_t io_num;
    uint8_t data[AXP2101_IRQ_STATUS_CNT];
    while (1){
        if (xQueueReceive(axp2101_evt_queue, &io_num, portMAX_DELAY)) {//wait for interrupt
            if (io_num==IOPIN_PMIC_PWR){
                axp2101_i2c_read_bytes(AXP2101_IRQ_STATUS1, AXP2101_IRQ_STATUS_CNT , data);
                check_axp2101_status(data);//数组名在传递给函数时会退化为指针
                ESP_LOGI("task_power_management","AXP2101_IRQsta is %x  %X  %X",data[0],data[1],data[2]);
                AXP2101_IRQStatus_clear();
                }
            printf("GPIO[%"PRIu32"] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
    vTaskDelete(NULL);
}

uint8_t AXP2101_bat_percentage(void)
{
    uint8_t data = 0xFF;
    axp2101_i2c_read_bytes(AXP2101_STATUS1, 1, &data);
    if (data && (1<<3))
    {
        axp2101_i2c_read_bytes(AXP2101_BATTERY_PERCENTAGE, 1, &data);
    }
    else
    {

    }

    return data;
}



// esp_err_t i2c_deinit(void)
// {
//     ESP_ERROR_CHECK(i2c_del_master_bus(i2c_bus_handle));
//     i2c_initialized = false;
//     return ESP_OK;
// }

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

static void check_axp2101_status(uint8_t *status_data)
{
    StatusCheckItem *item = axp2101_status_checks;
    while (item->message != NULL) {
        // Check the multi-bit status
        if ((item->mask & (item->mask - 1)) != 0) {  // Determine whether it is a multi-bit mask
            if ((status_data[item->byte_idx] & item->mask)) {//完全符合取if ((status_data[item->byte_idx] & item->mask) == item->mask) {
                AXP2101_print(item->message);
            }
        }
        // Check the single-bit status
        else {
            if (status_data[item->byte_idx] & item->mask) {
                AXP2101_print(item->message);
            }
        }
        item++;
    }
}


