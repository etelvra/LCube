#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "light_sleep.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "Pin_Definitions.h"

static const char *TAG = "System";

static TimerHandle_t lightsleep_inactivity_timer = NULL;
QueueHandle_t lightsleep_event_queue = NULL;
volatile bool enter_lightsleep = false;//每次读这个变量，都必须从内存重新读取每次写这个变量，都必须真实写回内存


#define INACTIVITY_TIMEOUT_MS         (60 * 1000)  // 30秒无操作进入睡眠
#define LIGHTSLEEP_WAKEUP_TIMER_US    (60*1000*1000 * 30)
#define GPIO_WAKEUP_NUM               IOPIN_PMIC_PWR
#define GPIO_WAKEUP_LEVEL             0

static void LightSleep_EVENTfunction_handler(TimerHandle_t pxTimer)
{
    enter_lightsleep = true;
}

void LightSleep_inactivity_timer_reset(void)
{
    if (xTimerReset(lightsleep_inactivity_timer, 0) == pdPASS) {
        ESP_LOGD(TAG, "Inactivity timer reset");
    }
}

void LightSleep_inactivity_timer_stop(void)
{
    xTimerStop(lightsleep_inactivity_timer, 0);
}

void LightSleep_wait_gpio_inactive(void)
{
    ESP_LOGI(TAG, "Waiting for GPIO%d to go high...\n", GPIO_WAKEUP_NUM);
    while (gpio_get_level(GPIO_WAKEUP_NUM) == GPIO_WAKEUP_LEVEL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t LightSleep_register_gpio_wakeup(void)
{
    /* Initialize GPIO */
    gpio_config_t config = {
            .pin_bit_mask = BIT64(GPIO_WAKEUP_NUM),
            .mode = GPIO_MODE_INPUT,
            .pull_down_en = false,
            .pull_up_en = false,
            .intr_type = GPIO_INTR_DISABLE
    };
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "Initialize GPIO%d failed", GPIO_WAKEUP_NUM);

    /* Enable wake up from GPIO */
    ESP_RETURN_ON_ERROR(gpio_wakeup_enable(GPIO_WAKEUP_NUM, GPIO_INTR_LOW_LEVEL),TAG, "Enable gpio wakeup failed");
    ESP_RETURN_ON_ERROR(esp_sleep_enable_gpio_wakeup(), TAG, "Configure gpio as wakeup source failed");

    /* Make sure the GPIO is inactive and it won't trigger wakeup immediately */
    LightSleep_wait_gpio_inactive();
    AMOLED_console_log(INFORM, false, TAG, "gpio wakeup source is ready");
    return ESP_OK;
}

esp_err_t LightSleep_register_timer_wakeup(void)
{
    ESP_RETURN_ON_ERROR(esp_sleep_enable_timer_wakeup(LIGHTSLEEP_WAKEUP_TIMER_US), TAG, "Configure timer as wakeup source failed");
    AMOLED_console_log(INFORM, false, TAG, "timer wakeup source is ready");
    return ESP_OK;
}

void task_lightsleep_management(void *param)
{
    vTaskDelay(10000);//防死机先别删
    uint32_t lightsleep_wakeup_event;
    if (lightsleep_event_queue == NULL){
        lightsleep_event_queue = xQueueCreate(10, sizeof(uint32_t));
    }

    //Create a software timer. Call back the lightsleep function when no operation long time
    lightsleep_inactivity_timer = xTimerCreate("inactivity_timer", pdMS_TO_TICKS(INACTIVITY_TIMEOUT_MS), pdFALSE, NULL, LightSleep_EVENTfunction_handler);
    if (lightsleep_inactivity_timer != NULL) {
        xTimerStart(lightsleep_inactivity_timer, 0);
        ESP_LOGI(TAG, "Inactivity timer started");
    } else {
        ESP_LOGE(TAG, "Failed to create inactivity timer");
    }

    /* Enable wakeup from light sleep by gpio */
    LightSleep_register_gpio_wakeup();
    /* Enable wakeup from light sleep by timer */
    LightSleep_register_timer_wakeup();
    while (1){
        if (xQueueReceive(lightsleep_event_queue, &lightsleep_wakeup_event, pdMS_TO_TICKS(1000))) {//wait for wake up event
            LightSleep_inactivity_timer_reset();
            AMOLED_console_log(INFORM, false, TAG, "Prolong waking time due to %d", lightsleep_wakeup_event);
        }
        if (enter_lightsleep)
        {
            enter_lightsleep = false;
            AMOLED_console_log(INFORM, false, TAG ,"Entering light sleep\n");
            /* To make sure the complete line is printed before entering sleep mode,
             * need to wait until UART TX FIFO is empty:
            */
            uart_wait_tx_idle_polling(CONFIG_ESP_CONSOLE_UART_NUM);
            AMOLED_console_log(INFORM, false, TAG ,"CONSOLE_DISPLAY_DISABLE");
            AMOLED_refresh();
            /* Get timestamp before entering sleep */
            int64_t t_before_us = esp_timer_get_time();

            /* Enter sleep mode */
            esp_light_sleep_start();
            /* Get timestamp after waking up from sleep */
            int64_t t_after_us = esp_timer_get_time();
            vTaskDelay(100);
            /* Determine wake up reason */
            const char* wakeup_reason;
            switch (esp_sleep_get_wakeup_cause()) {
                case ESP_SLEEP_WAKEUP_TIMER:
                    wakeup_reason = "timer";
                    break;
                case ESP_SLEEP_WAKEUP_GPIO:
                    wakeup_reason = "pin";
                    break;
                case ESP_SLEEP_WAKEUP_UART:
                    wakeup_reason = "uart";
                    /* Hang-up for a while to switch and execute the uart task
                     * Otherwise the chip may fall sleep again before running uart task */
                    vTaskDelay(1);
                    break;
                default:
                    wakeup_reason = "undefine";
                    break;
            }
            AMOLED_console_log(INFORM, false, TAG ,"Returned from light sleep,");
            AMOLED_console_log(INFORM, false, TAG ,"reason: %s, slept for %lld ms\n", wakeup_reason, (t_after_us - t_before_us) / 1000);
            //printf("Returned from light sleep, reason: %s, t=%lld ms, slept for %lld ms\n",wakeup_reason, t_after_us / 1000, (t_after_us - t_before_us) / 1000);
            if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
                /* Waiting for the gpio inactive, or the chip will continuously trigger wakeup*/
                LightSleep_wait_gpio_inactive();
            }
            LightSleep_inactivity_timer_reset();
        }
    }
    vTaskDelete(NULL);
}
