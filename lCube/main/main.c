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

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_co5300.h"
#include "IO_PIN_NUM.h"
#include "driver/spi_common.h"
// 分辨率设置
#define LCD_H_RES 384
#define LCD_V_RES 448
#define LCD_BPP   16  // RGB565

static SemaphoreHandle_t refresh_finish = NULL;
IRAM_ATTR static bool test_notify_refresh_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    xSemaphoreGiveFromISR(refresh_finish, &need_yield);
    return (need_yield == pdTRUE);
}

static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle)
{
    refresh_finish = xSemaphoreCreateBinary();
    //TEST_ASSERT_NOT_NULL(refresh_finish);

    uint16_t row_line = ((LCD_V_RES / LCD_BPP) << 1) >> 1;
    uint8_t byte_per_pixel = LCD_BPP / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * LCD_H_RES * byte_per_pixel, MALLOC_CAP_DMA);
    //TEST_ASSERT_NOT_NULL(color);

    for (int j = 0; j < LCD_BPP; j++) {
        for (int i = 0; i < row_line * LCD_H_RES; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), LCD_BPP) >> (k * 8)) & 0xff;
            }
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, LCD_H_RES, (j + 1) * row_line, color);
        xSemaphoreTake(refresh_finish, portMAX_DELAY);
    }
    free(color);
    vSemaphoreDelete(refresh_finish);
    vTaskDelay(pdMS_TO_TICKS(3000));
}



void init_lcd() {
    // 1. 配置QSPI总线
/*    spi_bus_config_t buscfg = {
        .sclk_io_num = IOPIN_QSPI_CLK,
        .mosi_io_num = IOPIN_QSPI_D_0,
        .miso_io_num = IOPIN_QSPI_Q_1,  // 未使用
        .quadwp_io_num = IOPIN_QSPI_WP_2,
        .quadhd_io_num = IOPIN_QSPI_HD_3,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * LCD_BPP / 8,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));*/
    const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(IOPIN_QSPI_CLK,
                                                                 IOPIN_QSPI_D_0,
                                                                 IOPIN_QSPI_Q_1,
                                                                 IOPIN_QSPI_WP_2,
                                                                 IOPIN_QSPI_HD_3,
                                                                 LCD_H_RES * LCD_V_RES * LCD_BPP / 8);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));



    // 2. 安装面板IO (QSPI模式)
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(IOPIN_QSPI_CS0, test_notify_refresh_ready, NULL);
/*    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = -1,  // QSPI模式下不需要DC引脚
        .cs_gpio_num = IOPIN_QSPI_CS0,
        .pclk_hz = 40 * 1000 * 1000,  // 40MHz
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,  // 同步传输
        .flags = {
            .dc_low_on_data = 0,
            .lsb_first = 0,
            .octal_mode = 0,
            .sio_mode = 0  // QSPI模式
        }
    };*/
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 3. 安装CO5300驱动 (QSPI模式)
    esp_lcd_panel_handle_t panel_handle = NULL;
    const co5300_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,  // 启用QSPI
        },
        //.init_cmds = NULL,  // 使用默认初始化序列
        //.init_cmds_size = 0
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = IOPIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BPP,
        .vendor_config = (void *) &vendor_config,
        //.flags = {.reset_active_high = 0}  // 复位低电平有效}
    };


    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io_handle, &panel_config, &panel_handle));

    // 4. 初始化面板
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    test_draw_bitmap(panel_handle);
    esp_lcd_panel_del(panel_handle);
    esp_lcd_panel_io_del(io_handle);
    spi_bus_free(LCD_HOST);
}




SemaphoreHandle_t SemaphoreHandle1=NULL;

TaskHandle_t task_led_indicator_handler;

void app_main(void)
{
    ledc_configer( );
    AXP2101_init( );
//    WIFI_STA_init( );
    vTaskDelay(1000);
//    SNTP_obtain_time();
    init_lcd();
    //SemaphoreHandle1=xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(task_led_indicator,"task1",2048,NULL,1,&task_led_indicator_handler,1);
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
