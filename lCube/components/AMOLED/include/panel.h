
#ifndef AMOLED_PANEL_H_
#define AMOLED_PANEL_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

/*!             Header files
 ******************************************************************************/
#define SCREEN_WIDTH  384 // 横屏宽度
#define SCREEN_HEIGHT 384 // 横屏高度

// 分辨率设置
#define LCD_H_RES 384  //Horizon
#define LCD_V_RES 448  //Vertical

#define BPP_COLOR_DEPTH RGB565
#define RGB565    16
#define RGB666    18
#define RGB888    24

#if BPP_COLOR_DEPTH == 16
typedef uint16_t pixel_t;
#define COLOR_TO_PIXEL(color) (((color >> 8) & 0x00FF) | ((color << 8) & 0xFF00))
#define PIXEL_BLACK     COLOR_TO_PIXEL(0x0000)
#define PIXEL_WHITE     0xFFFF
#define PIXEL_ERROR     COLOR_TO_PIXEL(0xF800)
#define PIXEL_INFO      COLOR_TO_PIXEL(0x15CF)
#define PIXEL_WARN      COLOR_TO_PIXEL(0xE722)
#elif BPP_COLOR_DEPTH == 24
typedef struct {
    uint8_t red, green, blue;
} pixel_t;
#define COLOR_TO_PIXEL(color) {(color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF}
#define PIXEL_BLACK     ((pixel_t){0x00, 0x00, 0x00})
#define PIXEL_WHITE     ((pixel_t){0xFF, 0xFF, 0xFF})
#define PIXEL_ERROR     ((pixel_t){0xDB, 0x6A, 0x6A})
#define PIXEL_INFO      ((pixel_t){0x0D, 0xBC, 0x79})
#define PIXEL_WARN      ((pixel_t){0xE5, 0xE5, 0x10})
#endif

#define CHAR_WIDTH    8    // 字符宽度(像素)
#define CHAR_HEIGHT  16    // 字符高度(像素)
#define CONSOLE_ROWS (SCREEN_HEIGHT / CHAR_HEIGHT -4)  // 24行
#define CONSOLE_COLS (SCREEN_WIDTH / CHAR_WIDTH)    // 56列
//console log level
#define NONE       0
#define ERROR      1
#define WARN       2
#define INFORM     3
#define DEBUG      4

typedef struct panel_console{
    esp_lcd_panel_handle_t panel;
    bool display_enable;
    char buffer[128];                   // +1 for null-terminator
    uint16_t row_pos;                   //current print line position
    uint16_t log_num;
    QueueHandle_t log_queue;
    pixel_t *line_buffers[CONSOLE_ROWS]; //circular single-line graphic buffer
} panel_console_t;

typedef struct console_log{              //Send structure to the queue
    uint8_t level;
    bool overwrite;
    char tag[8];
    char message[64];
    TickType_t timestamp;
} console_log_t;

extern esp_lcd_panel_handle_t amoled_panel_handle;
extern esp_lcd_touch_handle_t amoled_touch_handle;
extern SemaphoreHandle_t amoled_panel_mutex;
extern SemaphoreHandle_t amoled_touch_sem;

void AMOLED_DISPLAY_init(void);
esp_err_t AMOLED_panel_draw_bitmap_mutex(esp_lcd_panel_handle_t panel,
                                            int x_start, int y_start, int x_end, int y_end,
                                            const void *color_data);

void AMOLED_TOUCH_init(void);
void AMOLED_LVGL_init(void);

void AMOLED_refresh(void);
esp_err_t AMOLED_render_direction_set(bool portrait);
void AMOLED_console_log(uint8_t level, bool overwrite ,const char *tag, const char *format, ...);
void AMOLED_print_single_line(uint16_t x_pos, uint16_t y_pos, bool portrait, const char *text, ...);

void AMOLED_te_sync_init(void);
bool AMOLED_wait_te(uint32_t timeout_ms);






/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* AMOLED_PANEL_H_ */
