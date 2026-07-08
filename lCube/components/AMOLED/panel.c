#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"

#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst820.h"
#include "Pin_Definitions.h"
#include "font_8x16.h"
#include "panel.h"
#include "AXP2101.h"

static const char*TAG = "panel";

esp_lcd_panel_handle_t amoled_panel_handle = NULL;
esp_lcd_touch_handle_t amoled_touch_handle = NULL;
SemaphoreHandle_t amoled_panel_mutex = NULL;
SemaphoreHandle_t amoled_touch_mutex = NULL;

static i2c_bus_handle_t  i2c_tp_handle = NULL;

static panel_console_t console;//static struct panel_console console
static SemaphoreHandle_t refresh_finish = NULL;
extern const uint8_t font_8x16[95][16];// 字库引用 (95个ASCII字符 32-126)

IRAM_ATTR static bool test_notify_refresh_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(refresh_finish, &need_yield);
    return (need_yield == pdTRUE);
}

IRAM_ATTR static void touch_callback(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(amoled_touch_mutex, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t AMOLED_panel_draw_bitmap_mutex(esp_lcd_panel_handle_t panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    if (amoled_panel_mutex == NULL) {
        ESP_LOGW(TAG, "Amoled not initialized");
        return ESP_FAIL;
    }
    if (xSemaphoreTake(amoled_panel_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, x_start, y_start, x_end, y_end, color_data);
        xSemaphoreGive(amoled_panel_mutex);
        return ret;
    } else {
        ESP_LOGW(TAG, "Failed to acquire display mutex");
        return ESP_FAIL;
    }
}

void AMOLED_console_log(uint8_t level, bool overwrite, const char *tag, const char *format, ...) {
    if (amoled_panel_mutex == NULL || console.log_queue == NULL) {
        ESP_LOGW(TAG, "Console not initialized, dropping log message");
        return;
    }

    console_log_t console_log_buffer;
    console_log_buffer.level = level;
    console_log_buffer.overwrite = overwrite;
    console_log_buffer.timestamp = xTaskGetTickCount();
    //format tag
    strncpy(console_log_buffer.tag, tag, sizeof(console_log_buffer.tag));
    console_log_buffer.tag[sizeof(console_log_buffer.tag) - 1] = '\0';
    //format message
    va_list args;
    va_start(args, format);
    vsnprintf(console_log_buffer.message, sizeof(console_log_buffer.message) - 1, format, args);
    va_end(args);
    console_log_buffer.message[sizeof(console_log_buffer.message) - 1] = '\0';//prevent overflow

    //Send to the queue (non-blocking)
    if (xQueueSend(console.log_queue, &console_log_buffer, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "Log queue full, dropping message: %s", console_log_buffer.message);
    }
}

void AMOLED_print_single_line(uint16_t x_pos, uint16_t y_pos, bool portrait, const char *text, ...) {
    if (amoled_panel_mutex == NULL) {
        ESP_LOGW(TAG, "Amoled not initialized, dropping log message");
        return;
    }
    uint8_t byte_per_pixel = sizeof(pixel_t);
    const pixel_t bg_color = PIXEL_BLACK;
    const pixel_t fg_color = PIXEL_WHITE;
    char char_buffer[128];

    //format log text
    va_list args;
    va_start(args, text);
    vsnprintf(char_buffer, sizeof(char_buffer), text, args);
    va_end(args);

    pixel_t *single_line_buffer = (pixel_t *)heap_caps_malloc(LCD_H_RES * CHAR_HEIGHT * byte_per_pixel, MALLOC_CAP_DMA);
    if (!single_line_buffer) {
        ESP_LOGE(TAG, "Failed to allocate  single_line_buffer");
        return;
    }
    char_buffer[LCD_H_RES/CHAR_WIDTH - 1] = '\0';//prevent overflow
    printf("%s\n",char_buffer);

    //Render character
    for (int col = 0; col < strlen(char_buffer); col++) {
        char c = char_buffer[col];
        if (c < 32 || c > 126) c = ' '; // 非可打印字符

        const uint8_t *glyph = font_8x16[c - 32];

        if (portrait){//横竖屏不同渲染
            for (int x = 0; x < 16; x++) {
                for (int y = 0; y < 8; y++) {
                    //render the current pixe
                    int pos = (((x/8)*8+y) * strlen(char_buffer)* CHAR_WIDTH) + (col * CHAR_WIDTH) + x%8;
                    if (glyph[x] & (1 <<  y )) {
                        #if BPP_COLOR_DEPTH == 16
                        ((pixel_t *)single_line_buffer)[pos] = fg_color;
                        #elif BPP_COLOR_DEPTH == 24
                        memcpy(&single_line_buffer[pos * byte_per_pixel], &fg_color, byte_per_pixel);
                        #endif
                    }else{
                        #if BPP_COLOR_DEPTH == 16
                        ((pixel_t *)single_line_buffer)[pos] = bg_color;
                        #elif BPP_COLOR_DEPTH == 24
                        memcpy(&single_line_buffer[pos * byte_per_pixel], &bg_color, byte_per_pixel);
                        #endif
                    }
                }
            }
        }else{
            for (int x = 0; x < 16; x++) {
                for (int y = 0; y < 8; y++) {
                    //        整字符位置坐标+ 字符列转行坐标 + 字符行从上而下坐标
                    int pos = (strlen(char_buffer)-col)*128 -x%8*CHAR_HEIGHT-CHAR_HEIGHT + ((x/8)*8+y) ;
                    if (glyph[x] & (1 <<  y )) {
                        #if BPP_COLOR_DEPTH == 16
                        ((pixel_t *)single_line_buffer)[pos] = fg_color;
                        #elif BPP_COLOR_DEPTH == 24
                        memcpy(&single_line_buffer[pos * byte_per_pixel], &fg_color, byte_per_pixel);
                        #endif
                    }else{
                        #if BPP_COLOR_DEPTH == 16
                        ((pixel_t *)single_line_buffer)[pos] = bg_color;
                        #elif BPP_COLOR_DEPTH == 24
                        memcpy(&single_line_buffer[pos * byte_per_pixel], &bg_color, byte_per_pixel);
                        #endif
                    }
                }
            }
        }
    }
    if (portrait){
        AMOLED_panel_draw_bitmap_mutex(amoled_panel_handle, x_pos, y_pos, x_pos+strlen(char_buffer)*CHAR_WIDTH, y_pos + CHAR_HEIGHT, single_line_buffer);
    }else{
        AMOLED_panel_draw_bitmap_mutex(amoled_panel_handle, y_pos, LCD_V_RES-x_pos-strlen(char_buffer)*CHAR_WIDTH, y_pos + CHAR_HEIGHT, LCD_V_RES-x_pos, single_line_buffer);
    }
    heap_caps_free(single_line_buffer);
}

static void AMOLED_render_single_line(uint8_t level, const char *text) {//const:在该函数内禁止通过text指针修改它所指向的字符内容
    uint8_t byte_per_pixel = sizeof(pixel_t);
    const pixel_t bg_color = PIXEL_BLACK;
    pixel_t fg_color = PIXEL_WHITE;
    switch (level)
    {
    case ERROR:
        fg_color = PIXEL_ERROR;
        break;
    case WARN:
        fg_color = PIXEL_WARN;
        break;
    case DEBUG:
        fg_color = PIXEL_INFO;
        break;
    default:
        fg_color = PIXEL_WHITE;//(pixel_t){0xFB, 0xE4, 0xEE};
        break;
    }
    // Clear the line_buffer
    for (int i = 0; i < SCREEN_WIDTH * CHAR_HEIGHT; i++) {
        #if BPP_COLOR_DEPTH == 16
        ((pixel_t *)console.line_buffers[console.row_pos])[i] = bg_color;
        #elif BPP_COLOR_DEPTH == 24
        memcpy(&console.line_buffers[console.row_pos][i * byte_per_pixel], &bg_color, byte_per_pixel);
        #endif
    }

    //Draw character
    for (int col = 0; col < strlen(text); col++) {
        char c = text[col];
        if (c < 32 || c > 126) c = ' '; // 非可打印字符

        const uint8_t *glyph = font_8x16[c - 32];

        for (int x = 0; x < 16; x++) {
            for (int y = 0; y < 8; y++) {
                if (glyph[x] & (1 <<  y )) {
                    int pos = (((x/8)*8+y) * SCREEN_WIDTH) + (col * CHAR_WIDTH) + x%8;
                    //render the current pixe
                    #if BPP_COLOR_DEPTH == 16
                    ((pixel_t *)console.line_buffers[console.row_pos])[pos] = fg_color;
                    #elif BPP_COLOR_DEPTH == 24
                    memcpy(&console.line_buffers[console.row_pos][pos * byte_per_pixel], &fg_color, byte_per_pixel);
                    #endif
                }
            }
        }
        // for (int y = 0; y < CHAR_HEIGHT; y++) {//字库格式为自上而下16行数据
        //     for (int x = 0; x < CHAR_WIDTH; x++) {
        //         if (glyph[y] & (1 << (7 - x))) {
        //             int pos = (y * SCREEN_WIDTH) + (col * CHAR_WIDTH) + x;
        //             //render the current pixe
        //             #if BPP_COLOR_DEPTH == 16
        //             ((pixel_t *)console.line_buffers[console.row_pos])[pos] = fg_color;
        //             #elif BPP_COLOR_DEPTH == 24
        //             memcpy(&console.line_buffers[console.row_pos][pos * byte_per_pixel], &fg_color, byte_per_pixel);
        //             #endif
        //         }
        //     }
        // }
    }
}

static void AMOLED_console_display(const console_log_t *console_log, bool previous_state)//Use Pointers to deliver structures to save stack
{
    pixel_t *line_buffer_TS =NULL;
    //format log(tag + message)
    snprintf(console.buffer, sizeof(console.buffer), "[%s] %s", console_log->tag, console_log->message);
    console.buffer[CONSOLE_COLS] = '\0';

    switch (console_log->level)
    {
    case ERROR:
        ESP_LOGE( "AMOLED" ,"%s",console.buffer);      break;
    case WARN:
        ESP_LOGW( "AMOLED" ,"%s",console.buffer);      break;
    case DEBUG:
        ESP_LOGD( "AMOLED" ,"%s",console.buffer);      break;
    default:
        ESP_LOGI( "AMOLED" ,"%s",console.buffer);      break;
    }

    if (console.display_enable){
        if (!previous_state){//Deleting this line will print all the details
            //scroll display buffer(address rotation)
            if (console.row_pos == CONSOLE_ROWS - 1) {
                line_buffer_TS = console.line_buffers[0];
                for (int line = 0; line < CONSOLE_ROWS - 1; line++) {
                    console.line_buffers[line] = console.line_buffers[line + 1 ];
                    AMOLED_panel_draw_bitmap_mutex(console.panel, 0, line*CHAR_HEIGHT, SCREEN_WIDTH, line*CHAR_HEIGHT + CHAR_HEIGHT,
                                                        console.line_buffers[line]);
                }
                console.line_buffers[CONSOLE_ROWS - 1] = line_buffer_TS;
            } else {
                for (int line = 0; line <= console.row_pos; line++) {
                    AMOLED_panel_draw_bitmap_mutex(console.panel, 0, line*CHAR_HEIGHT, SCREEN_WIDTH, line*CHAR_HEIGHT + CHAR_HEIGHT,
                                                        console.line_buffers[line]);
                }
                console.row_pos++;
            }
        }
    //render and print new line
    AMOLED_render_single_line(console_log->level, console.buffer);
    AMOLED_panel_draw_bitmap_mutex(console.panel, 0, console.row_pos*CHAR_HEIGHT,
                                                SCREEN_WIDTH, console.row_pos*CHAR_HEIGHT + CHAR_HEIGHT,
                                                console.line_buffers[console.row_pos]);
    }
    console.log_num++;
}

static void task_console(void *param)
{
    console_log_t console_log_buffer;
    bool previous_state = false;
    while (1){
        if (xQueueReceive(console.log_queue, &console_log_buffer, portMAX_DELAY)) {//wait for log massage :)
            if (!strcmp(console_log_buffer.message, "CONSOLE_DISPLAY_ENABLE")){
                console.display_enable = true;
            }else if(!strcmp(console_log_buffer.message, "CONSOLE_DISPLAY_DISABLE")){
                console.display_enable = false;
                continue;
            }
            AMOLED_console_display(&console_log_buffer, previous_state);
            previous_state = console_log_buffer.overwrite;
        }
        //short delay to free up the CPU for improve the overall response of the system
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}

esp_err_t AMOLED_console_init(esp_lcd_panel_handle_t panel) {
    console.panel = panel;
    console.display_enable = true;
    console.row_pos = 0;
    console.log_num = 0;
    uint8_t byte_per_pixel = sizeof(pixel_t);

    console.log_queue = xQueueCreate(10, sizeof(console_log_t));
    //Allocate the single-line graphic buffer
    for (int line = 0; line < CONSOLE_ROWS; line++) {
        console.line_buffers[line] = (pixel_t *)heap_caps_malloc(SCREEN_WIDTH * CHAR_HEIGHT * byte_per_pixel, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    }
    xTaskCreatePinnedToCore(task_console,"console",4096,NULL,16,NULL,1);
    return ESP_OK;
}

//clear the contents of the screen registers
void AMOLED_refresh(void) {
    uint16_t *refresh_buffer = heap_caps_calloc(1,CHAR_HEIGHT* LCD_H_RES * BPP_COLOR_DEPTH /8, MALLOC_CAP_DMA);
    vTaskDelay(pdMS_TO_TICKS(50));
    if (xSemaphoreTake(amoled_panel_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        esp_lcd_panel_swap_xy(amoled_panel_handle, 0);
        esp_lcd_panel_mirror(amoled_panel_handle, 1, 1);
        for (int i = 0; i < LCD_V_RES/CHAR_HEIGHT ; i++) {
            esp_lcd_panel_draw_bitmap(amoled_panel_handle, 0, CHAR_HEIGHT * i, LCD_H_RES, CHAR_HEIGHT * i + CHAR_HEIGHT, refresh_buffer);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        esp_lcd_panel_swap_xy(amoled_panel_handle, 1);
        esp_lcd_panel_mirror(amoled_panel_handle, 1, 0);
        xSemaphoreGive(amoled_panel_mutex);

    } else {
            ESP_LOGW(TAG, "Failed to acquire display mutex");
    }
    heap_caps_free(refresh_buffer);
}

void AMOLED_DISPLAY_init() {
    //Configure the QSPI bus
    const spi_bus_config_t buscfg = {
        .sclk_io_num = IOPIN_QSPI_CLK,
        .mosi_io_num = IOPIN_QSPI_D_0,
        .miso_io_num = IOPIN_QSPI_Q_1,
        .quadwp_io_num = IOPIN_QSPI_WP_2,
        .quadhd_io_num = IOPIN_QSPI_HD_3,
        .max_transfer_sz = LCD_H_RES * 64 * BPP_COLOR_DEPTH / 8,
        .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS | SPICOMMON_BUSFLAG_QUAD
    };
    /*const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(IOPIN_QSPI_CLK,
                                                                 IOPIN_QSPI_D_0,
                                                                 IOPIN_QSPI_Q_1,
                                                                 IOPIN_QSPI_WP_2,
                                                                 IOPIN_QSPI_HD_3,
                                                                 LCD_H_RES * LCD_V_RES * BPP_COLOR_DEPTH / 8);*/
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    amoled_panel_mutex = xSemaphoreCreateMutex();
    //Install panel IO (QSPI mode)
    esp_lcd_panel_io_handle_t io_handle = NULL;
    refresh_finish = xSemaphoreCreateBinary();
    if (refresh_finish == NULL) {
        ESP_LOGE(TAG, "Failed to create refresh semaphore");
        return;
    }
    //Configure as the default configuration for CO5300
    const esp_lcd_panel_io_spi_config_t io_config = CO5300_PANEL_IO_QSPI_CONFIG(IOPIN_QSPI_CS0, test_notify_refresh_ready, NULL);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    //Install the CO5300 driver (QSPI mode)
    const co5300_vendor_config_t vendor_config = {
        .flags = {
            .use_qspi_interface = 1,  // 启用QSPI
        },
        .init_cmds = NULL,  //Use the default initialization sequence
        //.init_cmds_size = 0
    };

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = IOPIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BPP_COLOR_DEPTH,
        .vendor_config = (void *) &vendor_config,
        //.flags = {.reset_active_high = 0}  // 复位低电平有效}
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io_handle, &panel_config, &amoled_panel_handle));

    //Initialize the panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(amoled_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(amoled_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(amoled_panel_handle, true));
    //clear the contents of the screen registers
    uint16_t *refresh_buffer = heap_caps_calloc(1,CHAR_HEIGHT* LCD_H_RES * BPP_COLOR_DEPTH /8, MALLOC_CAP_DMA);
    for (int i = 0; i < LCD_V_RES/CHAR_HEIGHT ; i++) {
        AMOLED_panel_draw_bitmap_mutex(amoled_panel_handle, 0, CHAR_HEIGHT * i, LCD_H_RES, CHAR_HEIGHT * i + CHAR_HEIGHT, refresh_buffer);
        //vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    heap_caps_free(refresh_buffer);

    esp_lcd_panel_swap_xy(amoled_panel_handle, 1);
    esp_lcd_panel_mirror(amoled_panel_handle, 1, 0);

    AMOLED_console_init(amoled_panel_handle);

    AMOLED_console_log(INFORM, false ,"","   The display panel has been initialized");
    vTaskDelay(pdMS_TO_TICKS(50));
    AMOLED_console_log(DEBUG, false ,"","--------Start initializing the system---------");
    //test_draw_bitmap(amoled_panel_handle);

    // esp_lcd_panel_del(amoled_panel_handle);
    // esp_lcd_panel_io_del(io_handle);
    // spi_bus_free(LCD_HOST);
}

void AMOLED_TOUCH_init(void)
{
    const i2c_config_t i2c_tp_config = {
        .mode = I2C_MODE_MASTER,
        .scl_io_num    = IOPIN_TP_SCL,
        .sda_io_num    = IOPIN_TP_SDA,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .clk_flags     = I2C_CLK_SRC_DEFAULT,
        .master.clk_speed = 400*1000,
    };
    i2c_tp_handle = i2c_bus_create(I2C_TP_PORT, &i2c_tp_config);

    amoled_touch_mutex = xSemaphoreCreateBinary();
    //Configure the touch IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG();
#ifdef CONFIG_I2C_BUS_BACKWARD_CONFIG
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_TP_PORT, &io_config, &io_handle));//未验证
#else
    io_config.scl_speed_hz=400000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(i2c_bus_get_internal_bus_handle(i2c_tp_handle), &io_config, &io_handle));
#endif

    //Configure the touch controller
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
        .interrupt_callback = touch_callback,
    };
    //Create the touch controller
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst820(io_handle, &touch_config, &amoled_touch_handle));
    esp_lcd_touch_set_swap_xy(amoled_touch_handle, 1);
    esp_lcd_touch_set_mirror_x(amoled_touch_handle, 1);
}




// 读取并打印触摸坐标
void touch_read_task(void *arg) {
    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint8_t point_num = 0;

    while (1) {
        // 读取触摸数据
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_touch_read_data(amoled_touch_handle));

        // 获取坐标
        if (esp_lcd_touch_get_coordinates(amoled_touch_handle, x, y, NULL, &point_num, 1)) {
            if (point_num > 0) {
                printf("触摸坐标: X=%d, Y=%d\n", x[0], y[0]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz采样率
    }
}


static void test_draw_bitmap(esp_lcd_panel_handle_t panel_handle)
{
    uint16_t row_line = ((LCD_V_RES / BPP_COLOR_DEPTH) << 1) >> 1;
    uint8_t byte_per_pixel = BPP_COLOR_DEPTH / 8;
    uint8_t *color = (uint8_t *)heap_caps_calloc(1, row_line * LCD_H_RES * byte_per_pixel, MALLOC_CAP_DMA);

    for (int j = 0; j < BPP_COLOR_DEPTH; j++) {
        for (int i = 0; i < row_line * LCD_H_RES; i++) {
            for (int k = 0; k < byte_per_pixel; k++) {
                color[i * byte_per_pixel + k] = (SPI_SWAP_DATA_TX(BIT(j), BPP_COLOR_DEPTH) >> (k * 8)) & 0xff;
            }
        }
        esp_lcd_panel_draw_bitmap(panel_handle, 0, j * row_line, LCD_H_RES, (j + 1) * row_line, color);
        //xSemaphoreTake(refresh_finish, portMAX_DELAY);
    }
    heap_caps_free(color);
    //vSemaphoreDelete(refresh_finish);
    vTaskDelay(pdMS_TO_TICKS(3000));
}
