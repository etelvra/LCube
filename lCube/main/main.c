#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "LED.h"
#include "AXP2101.h"
#include "WIFI.h"
#include "panel.h"
#include "lvgl_display.h"
#include "MIC.h"
#include "BM_SENSOR.h"
#include "light_sleep.h"

static const char *TAG = "main";
TaskHandle_t task_led_indicator_handler;


void app_main(void)
{
    PMIC_init();
    ledc_configer();
    AMOLED_DISPLAY_init();
    AMOLED_TOUCH_init();

    xTaskCreatePinnedToCore(task_lightsleep_management,"lightsleep_management",8192,NULL,20,NULL,1);

    //xTaskCreatePinnedToCore(task_adc_mic_listen,"Initialize the mic and keep listening", 4096,NULL,4,NULL,0);
    //xTaskCreatePinnedToCore(task_fft_process,"task_fft",8192,NULL,2,NULL,1);

    ledc_configer();
    xTaskCreatePinnedToCore(task_led_indicator,"task1",2048,NULL,1,&task_led_indicator_handler,1);

    //BM_SENSOR_init();
    AMOLED_LVGL_init();

    vTaskDelay(10000);
    vTaskDelete(task_led_indicator_handler);
    //xTaskCreatePinnedToCore(task_http_test,"task1",8192,NULL,1,NULL,1);
}

