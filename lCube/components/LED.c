#include <stdio.h>
#include "LED.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"



void ledc_configer(void)
{
    gpio_config_t led_configer = {
        .pin_bit_mask = (1<<GPIO_NUM_0),
        .pull_down_en =GPIO_PULLDOWN_DISABLE,
        .pull_up_en   =GPIO_PULLUP_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_configer);
    ledc_timer_config_t ledc_timer ={
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num  = LEDC_TIMER_0,
        .clk_cfg    = LEDC_USE_RC_FAST_CLK,
        .freq_hz    = 1024,
        .duty_resolution =  LEDC_TIMER_10_BIT,
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel ={
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .gpio_num   = GPIO_NUM_0,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .intr_type  = LEDC_INTR_DISABLE,
    };
    ledc_channel_config(&ledc_channel);
    ledc_fade_func_install(0);
}


void task_led_indicator(void *param)
{
    while (1)
    {
        ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,1023,0);
        vTaskDelay(500);
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0,500);
        ledc_fade_start(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,LEDC_FADE_NO_WAIT);
    }
    vTaskDelete(NULL);
}
