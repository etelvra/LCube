#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "LED.h"
#include "AXP2101.h"
#include "WIFI.h"
#include "HTTP.h"
#include "TimeStamp.h"
//#include "panel.h"
//#include "mic.h"
#include "BM_SENSOR.h"

SemaphoreHandle_t SemaphoreHandle1=NULL;
//i2c_master_dev_handle_t bmp280_i2c_dev_hd = NULL;
TaskHandle_t task_led_indicator_handler;

void app_main(void)
{
    ledc_configer( );
    AXP2101_init( );
    BM_SENSOR_init();

xTaskCreatePinnedToCore(task_led_indicator,"task1",2048,NULL,1,&task_led_indicator_handler,1);
//    WIFI_STA_init( );
//    SNTP_obtain_time();
//    init_lcd();
//    ADC_MIC_init();
    //ADC_MIC_do();
//    AMOLED_TP_init();
//    printf("触摸控制器初始化完成\n");

    // 创建触摸读取任务
//    xTaskCreate(touch_read_task, "touch_task", 4096, NULL, 5, NULL);

    //SemaphoreHandle1=xSemaphoreCreateBinary();

//xTaskCreate(&task_wifi_beacon_spam, "task_wifi_beacon_spam", 4096, NULL, 3, NULL);
    vTaskDelay(10000);
    vTaskDelete(task_led_indicator_handler);
    //xTaskCreatePinnedToCore(task_http_test,"task1",8192,NULL,1,NULL,1);
}



/*

void task_Semaphoretake(void *param)
{
    while(1)
    {
        if(pdTRUE == xSemaphoreTake(SemaphoreHandle1,1))
        {
            ESP_LOGI("task_semaphoretake","semaphore value is %d",xSemaphoreTake(SemaphoreHandle1,100));
        }
        else
        {
            ESP_LOGI("task_semaphoretake","semaphore false");
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
void task_Semaphoregive(void *param)
{
    while(1)
    {
        xSemaphoreGive(SemaphoreHandle1);
        ESP_LOGI("task_semaphoregive","semaphore value seted");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}


*/
