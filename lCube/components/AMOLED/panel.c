#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/i2c.h"

#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst820.h"
#include "Pin_Definitions.h"


i2c_master_dev_handle_t amoled_tp_i2c_dev_hd = NULL;
esp_lcd_touch_handle_t amoled_touch_handle = NULL;
i2c_master_bus_handle_t i2c_handle = NULL;


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
    spi_bus_config_t buscfg = {
        .sclk_io_num = IOPIN_QSPI_CLK,
        .mosi_io_num = IOPIN_QSPI_D_0,
        .miso_io_num = IOPIN_QSPI_Q_1,  // 未使用
        .quadwp_io_num = IOPIN_QSPI_WP_2,
        .quadhd_io_num = IOPIN_QSPI_HD_3,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * LCD_BPP / 8,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS
    };

    /*const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(IOPIN_QSPI_CLK,
                                                                 IOPIN_QSPI_D_0,
                                                                 IOPIN_QSPI_Q_1,
                                                                 IOPIN_QSPI_WP_2,
                                                                 IOPIN_QSPI_HD_3,
                                                                 LCD_H_RES * LCD_V_RES * LCD_BPP / 8);*/
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


void AMOLED_TP_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = IOPIN_TP_SDA,
        .scl_io_num = IOPIN_TP_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,  // I2C时钟频率
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_TP_PORT, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_TP_PORT, conf.mode, 0, 0, 0));


/*    const i2c_device_config_t amoled_tp_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS,
        .scl_speed_hz = 200000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_handle, &amoled_tp_config, &amoled_tp_i2c_dev_hd)); */

    // 配置面板I2C IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG();

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)i2c_handle, &io_config, &io_handle));

    // 配置触摸控制器
    esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = IOPIN_TP_RST,
        .int_gpio_num = IOPIN_TP_IRQ,
        .levels = {
            .reset = 0,      // 复位电平
            .interrupt = 0,  // 中断触发电平 (0=下降沿)
        },
        .flags = {
            .swap_xy = 0,    // 是否交换XY坐标
            .mirror_x = 0,   // 是否镜像X坐标
            .mirror_y = 0,   // 是否镜像Y坐标
        },
    };

    // 创建触摸控制器实例
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst820(io_handle, &touch_config, &amoled_touch_handle));
}


// 读取并打印触摸坐标
void touch_read_task(void *arg) {
    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint8_t point_num = 0;

    while (1) {
        // 读取触摸数据
        ESP_ERROR_CHECK(esp_lcd_touch_read_data(amoled_touch_handle));

        // 获取坐标
        if (esp_lcd_touch_get_coordinates(amoled_touch_handle, x, y, NULL, &point_num, 1)) {
            if (point_num > 0) {
                printf("触摸坐标: X=%d, Y=%d\n", x[0], y[0]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz采样率
    }
}
