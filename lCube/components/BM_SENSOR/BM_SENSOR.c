#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#include "AXP2101.h"
#include "Pin_Definitions.h"
#include "common/common.h"
#include "bmi270.h"
#include "BMP280/bmp280.h"
#include "bmm150_aux_adapter.h"
#include "BM_SENSOR.h"

/*! Earth's gravity in m/s^2 */
#define GRAVITY_EARTH       (9.80665f)

/*! Macros to select the sensors */
#define ACCEL               UINT8_C(0x00)
#define GYRO                UINT8_C(0x01)
#define BMM150_I2C_ADDRESS_DEFAULT (0x10)

static bmi270_handle_t bmi270_device_handle = NULL;
static bmp280_handle_t bmp280_device_handle = NULL;
static bmm150_aux_handle_t bmm150_device_handle;

//Create the interrupt service function
QueueHandle_t bmi270_evt_queue = NULL;
static void IRAM_ATTR IMU_IRQfunction_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(bmi270_evt_queue, &gpio_num, NULL);
}
static int8_t set_feature_config(struct bmi2_dev *bmi2_dev);
static void task_bmi270_motion_detection(void *param);
static void task_bmp280_pressure_temperature_monitor(void *param);
static void task_bmm150_magnetic_monitor(void *param);

esp_err_t BM_SENSOR_init(void)
{
    esp_err_t ret = ESP_OK;
    i2c_bus_init();
    //configure GPIO for bmi270 interrupt
    const gpio_config_t IMU_irq_config = {
        .pin_bit_mask = (1ULL<< IOPIN_BMI2_INT1),  //1ULL must be used to prevent insufficient bit width
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .mode         = GPIO_MODE_INPUT,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&IMU_irq_config);
    //gpio_install_isr_service(ESP_INTR_FLAG_LOWMED|ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_EDGE);
    gpio_isr_handler_add(IOPIN_BMI2_INT1, IMU_IRQfunction_handler, (void*) IOPIN_BMI2_INT1);
    bmi270_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    bmi270_i2c_config_t i2c_bmi270_config = {
        .i2c_handle = i2c_bus_handle,
        .i2c_addr = BMI270_I2C_ADDRESS,
    };
    ret = bmi270_sensor_create(&i2c_bmi270_config, &bmi270_device_handle);

    bmp280_device_handle = bmp280_create(i2c_bus_handle, BMP280_I2C_ADDRESS);
    if(bmp280_default_init(bmp280_device_handle) != ESP_OK) {
        bmp280_delete(bmp280_device_handle);
    }else {
        //xTaskCreatePinnedToCore(task_bmp280_pressure_temperature_monitor,"task_bmp280_pressure_temperature_monitor",4096,NULL,3,NULL,1);
    }

    xTaskCreatePinnedToCore(task_bmi270_motion_detection,"bmi270_motion_detection",2048,NULL,3,NULL,1);
    //xTaskCreatePinnedToCore(task_bmm150_magnetic_monitor,"task_bmm150_magnetic_monitor",4096,NULL,3,NULL,1);
    return ESP_OK;
}

static void task_bmi270_motion_detection(void *param)
{
    uint32_t io_num;
    /* Status of api are returned to this variable. */
    int8_t rslt;

    /* Accel sensor and no-motion feature are listed in array. */
    uint8_t sens_list[2] = { BMI2_ACCEL, BMI2_ANY_MOTION };

    /* Variable to get no-motion interrupt status. */
    uint16_t int_status = 0;

    /* Select features and their pins to be mapped to. */
    struct bmi2_sens_int_config sens_int = { .type = BMI2_ANY_MOTION, .hw_int_pin = BMI2_INT1 };

    /* Enable the selected sensors. */
    rslt = bmi270_sensor_enable(sens_list, 2, bmi270_device_handle);
    bmi2_error_codes_print_result(rslt);
    if (rslt == BMI2_OK) {
        /* Set feature configurations for no-motion. */
        rslt = set_feature_config(bmi270_device_handle);
        bmi2_error_codes_print_result(rslt);

        if (rslt == BMI2_OK) {
            /* Map the feature interrupt for no-motion. */
            rslt = bmi270_map_feat_int(&sens_int, 1, bmi270_device_handle);
            bmi2_error_codes_print_result(rslt);
            /* Loop to get no-motion interrupt. */
            do {
                if (xQueueReceive(bmi270_evt_queue, &io_num, portMAX_DELAY)) {//wait for interrupt
                    if (io_num==IOPIN_BMI2_INT1){
                    /* Clear buffer. */
                    int_status = 0;
                    /* To get the interrupt status of any-motion. */
                    rslt = bmi2_get_int_status(&int_status, bmi270_device_handle);
                    bmi2_error_codes_print_result(rslt);
                    /* To check the interrupt status of any-motion. */
                    if (int_status & BMI270_ANY_MOT_STATUS_MASK) {
                        printf("Any-motion interrupt is generated\n");
                        break;//Exit when motion is detected
                    }
                    } else {
                    vTaskDelay(pdMS_TO_TICKS(100));
                    }
                }
            } while (rslt == BMI2_OK);
        }
    }
    gpio_isr_handler_remove(IOPIN_BMI2_INT1);
    vTaskDelete(NULL);
}

static void task_bmp280_pressure_temperature_monitor(void *param)
{
    float pressure = 0.0f;
    float temperature = 0.0f;
    while (1){
        if (ESP_OK == bmp280_read_pressure(bmp280_device_handle, &pressure))
        {
            ESP_LOGI("bmp280", "pressure:%f ", pressure);
        }
        if (ESP_OK == bmp280_read_temperature(bmp280_device_handle, &temperature))
        {
            ESP_LOGI("bmp280", "temperature:%f ", temperature);
        }
        vTaskDelay(1000);
    }
    vTaskDelete(NULL);
}

/*!
 * @brief This internal API is used to set configurations for any-motion.
 */
static int8_t set_feature_config(struct bmi2_dev *bmi2_dev)
{

    /* Status of api are returned to this variable. */
    int8_t rslt;

    /* Structure to define the type of sensor and its configurations. */
    struct bmi2_sens_config config;

    /* Interrupt pin configuration */
    struct bmi2_int_pin_config pin_config = { 0 };

    /* Configure the type of feature. */
    config.type = BMI2_ANY_MOTION;

    /* Get default configurations for the type of feature selected. */
    rslt = bmi270_get_sensor_config(&config, 1, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    rslt = bmi2_get_int_pin_config(&pin_config, bmi2_dev);
    bmi2_error_codes_print_result(rslt);

    if (rslt == BMI2_OK) {
        /* NOTE: The user can change the following configuration parameters according to their requirement. */
        /* 1LSB equals 20ms. Default is 100ms, setting to 80ms. */
        config.cfg.any_motion.duration = 0x04;

        /* 1LSB equals to 0.48mg. Default is 83mg, setting to 50mg. */
        config.cfg.any_motion.threshold = 0x68;

        /* Set new configurations. */
        rslt = bmi270_set_sensor_config(&config, 1, bmi2_dev);
        bmi2_error_codes_print_result(rslt);

        /* Interrupt pin configuration */
        pin_config.pin_type = BMI2_INT1;
        pin_config.int_latch = BMI2_INT_NON_LATCH;
        pin_config.pin_cfg[0].input_en = BMI2_INT_INPUT_DISABLE;
        pin_config.pin_cfg[0].lvl = BMI2_INT_ACTIVE_LOW;
        pin_config.pin_cfg[0].od = BMI2_INT_OPEN_DRAIN;
        pin_config.pin_cfg[0].output_en = BMI2_INT_OUTPUT_ENABLE;

        rslt = bmi2_set_int_pin_config(&pin_config, bmi2_dev);
        bmi2_error_codes_print_result(rslt);
    }

    return rslt;
}

static int8_t BMI270_AUX_interface_init(struct bmi2_dev *bmi2_dev, uint8_t aux_device_addr)
{
    int8_t rslt;
    /* Configure AUX interface parameters, enable AUX and set to manual mode, specify BMM150 I2C address */
    struct bmi2_sens_config sens_cfg = {0};
    sens_cfg.type = BMI2_AUX;
    sens_cfg.cfg.aux.aux_en = 1;              /* Enable AUX */
    sens_cfg.cfg.aux.manual_en = 1;           /* Manual mode */
    sens_cfg.cfg.aux.fcu_write_en = 0;
    sens_cfg.cfg.aux.man_rd_burst = 1;
    sens_cfg.cfg.aux.aux_rd_burst = 1;
    sens_cfg.cfg.aux.odr = 2;                 /* 2=100Hz */
    sens_cfg.cfg.aux.offset = 0;
    sens_cfg.cfg.aux.i2c_device_addr = aux_device_addr;  /* BMM150 default I2C address */
    rslt = bmi2_set_sensor_config(&sens_cfg, 1, bmi2_dev);
    if (rslt != BMI2_OK) {
        ESP_LOGE("AUX", "BMI270 AUX config failed: %d", rslt);
        return rslt;
    }
    ESP_LOGI("AUX", "BMI270 AUX interface configured");
    return BMI2_OK;
}

static void task_bmm150_magnetic_monitor(void *param)
{
    if (!bmi270_device_handle) {
        ESP_LOGE("BMM150_magnetometer", "Invalid BMI270 device pointer");
        vTaskDelete(NULL);
    }
    if(BMI270_AUX_interface_init(bmi270_device_handle, BMM150_I2C_ADDRESS_DEFAULT) != BMI2_OK) {
        vTaskDelete(NULL);
    }
    /* Configure BMM150 AUX adapter */
    bmm150_aux_config_t config = {
        .bmi2_dev = bmi270_device_handle,
        .i2c_addr = BMM150_I2C_ADDRESS_DEFAULT,        /* BMM150 default address */
        .chip_id_reg = 0x40,                           /* BMM150 chip ID register */
        };
    /* Initialize BMM150 AUX adapter */
    int8_t rslt = bmm150_aux_adapter_init(&config, &bmm150_device_handle);
    if (rslt != BMM150_OK) {
        ESP_LOGE("BMM150_AUX", "BMM150 AUX adapter init failed: %d", rslt);
        vTaskDelete(NULL);
    }

    /* Configure BMM150 settings */
    struct bmm150_settings settings = {0};
    settings.pwr_mode = BMM150_POWERMODE_NORMAL;  /* Normal power mode */
    settings.data_rate = BMM150_DATA_RATE_10HZ;   /* 10Hz data rate */
    settings.xy_rep = 9;                          /* XY repetition */
    settings.z_rep = 15;                          /* Z repetition */

    rslt = bmm150_aux_adapter_configure(&bmm150_device_handle, &settings);
    if (rslt != BMM150_OK) {
        ESP_LOGE("BMM150_AUX", "BMM150 configure failed: %d", rslt);
        bmm150_aux_adapter_deinit(&bmm150_device_handle);
        vTaskDelete(NULL);
    }
    /* Read magnetometer data */
    struct bmm150_mag_data mag_data = {0};
    while (1){
        rslt = bmm150_aux_adapter_read_mag_data(&bmm150_device_handle, &mag_data);
        if (rslt == BMM150_OK) {
            /* Calculate heading and strength */
            float heading = bmm150_aux_adapter_calculate_heading(mag_data.x, mag_data.y);
            float strength = bmm150_aux_adapter_calculate_strength(mag_data.x, mag_data.y, mag_data.z);
            const char* direction = bmm150_aux_adapter_get_direction(heading);

            ESP_LOGI("BMM150_AUX", "Magnetometer Data:");
            ESP_LOGI("BMM150_AUX", "  X: %.2f uT", (float)mag_data.x);
            ESP_LOGI("BMM150_AUX", "  Y: %.2f uT", (float)mag_data.y);
            ESP_LOGI("BMM150_AUX", "  Z: %.2f uT", (float)mag_data.z);
            ESP_LOGI("BMM150_AUX", "  Heading: %.1f (%s)", heading, direction);
            ESP_LOGI("BMM150_AUX", "  Strength: %.2f uT", strength);

            /* Check if magnetic field is normal */
            if (bmm150_aux_adapter_is_field_normal(strength)) {
                ESP_LOGI("BMM150_AUX", "  Magnetic field is normal");
            } else {
                ESP_LOGW("BMM150_AUX", "  Magnetic field is abnormal (%.1f uT)", strength);
            }
        } else {
            ESP_LOGE("BMM150_AUX", "Failed to read magnetometer data: %d", rslt);
        }
        vTaskDelay(1000);
    }
    bmm150_aux_adapter_deinit(&bmm150_device_handle);
    vTaskDelete(NULL);
}
