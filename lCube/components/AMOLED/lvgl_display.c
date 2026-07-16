#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl_display.h"

static const char*TAG = "ui";

LV_IMG_DECLARE(gif_dani);
static lv_obj_t *s_boot_gif = NULL;

static SemaphoreHandle_t s_lvgl_mutex = NULL;

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

#if BPP_COLOR_DEPTH == 24
    uint8_t *to = (uint8_t *)color_map;
    uint8_t temp = 0;
    uint16_t pixel_num = (offsetx2 - offsetx1 + 1) * (offsety2 - offsety1 + 1);

    // Special dealing for first pixel
    temp = color_map[0].ch.blue;
    *to++ = color_map[0].ch.red;
    *to++ = color_map[0].ch.green;
    *to++ = temp;
    // Normal dealing for other pixels
    for (int i = 1; i < pixel_num; i++) {
        *to++ = color_map[i].ch.red;
        *to++ = color_map[i].ch.green;
        *to++ = color_map[i].ch.blue;
    }
#endif

    // copy a buffer's content to a specific area of the display
    AMOLED_panel_draw_bitmap_mutex(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

static void lvgl_touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)drv->user_data;
    assert(tp);

    uint16_t tp_x = 0;
    uint16_t tp_y = 0;
    uint8_t tp_cnt = 0;

    /* Read data from touch controller into memory */
    if (xSemaphoreTake(amoled_touch_mutex, 0) == pdTRUE) {
        esp_lcd_touch_read_data(tp);
    }

    /* Read data from touch controller */
    bool tp_pressed = esp_lcd_touch_get_coordinates(tp, &tp_x, &tp_y, NULL, &tp_cnt, 1);
    if (tp_pressed && tp_cnt > 0) {
        data->point.x = tp_x;
        data->point.y = tp_y;
        data->state = LV_INDEV_STATE_PRESSED;
        ESP_LOGD(TAG, "Touch position: %d,%d", tp_x, tp_y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    if (tp_x){
        xQueueSendFromISR(lightsleep_event_queue, &tp_x, NULL);
    }
}

/* -------------------- LVGL Tick 定时器回调 -------------------- */
static void lvgl_tick_cb(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

/* -------------------- 线程安全：LVGL 锁封装 -------------------- */
bool lvgl_lock(int timeout_ms)
{
    assert(s_lvgl_mutex && "AMOLED_LVGL_init must be called first");
    const TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(s_lvgl_mutex, ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
    assert(s_lvgl_mutex && "AMOLED_LVGL_init must be called first");
    xSemaphoreGive(s_lvgl_mutex);
}

/* -------------------- 可选：在当前活动屏挂一个启动 GIF -------------------- */
static void ui_attach_boot_gif(void)
{
    lv_obj_t *active = lv_scr_act();
    if (!active) {
        return;
    }

    s_boot_gif = lv_gif_create(active);
    lv_gif_set_src(s_boot_gif, &gif_dani);
    lv_obj_align(s_boot_gif, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

/* -------------------- UI 初始化后统一做一次“页面初态”配置 -------------------- */
static void ui_post_init_default_state(void)
{
    /*
     * 这里先留空，作为统一扩展点：
     * - 后续可放：默认隐藏某些面板、默认文案、状态灯颜色等。
     * - 建议你只在这里做“UI状态初始化”，不要和驱动初始化混写。
     */
}

/* -------------------- LVGL 主任务 -------------------- */
void lvgl_clock_update_cb(lv_timer_t * timer)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    setenv("TZ", "CST-8", 1);
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);

    char time_str[9];
    sprintf(time_str, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    char date_str[32];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", timeinfo.tm_year +1900, timeinfo.tm_mon +1, timeinfo.tm_mday);

    lv_label_set_text_fmt(ui_timelabel, "%s", time_str);
    lv_label_set_text_fmt(ui_dataLabel, "%s", date_str);
}

void LVGL_timer_screen1(void)
{
    lv_timer_t * timer = lv_timer_create(lvgl_clock_update_cb, 500, NULL);
    lvgl_clock_update_cb(timer);
//    ui_attach_boot_gif();
}

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");

    ui_init();
    LVGL_timer_screen1();

    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void AMOLED_LVGL_init(void)
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions
    static lv_indev_drv_t indev_drv;    // Input device driver (Touch)

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    ui_post_init_default_state();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = heap_caps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    assert(buf1);//确认buffer申请成功
    lv_color_t *buf2 = heap_caps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, SCREEN_WIDTH * SCREEN_HEIGHT);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    //disp_drv.rounder_cb = example_lvgl_rounder_cb;
    //disp_drv.drv_update_cb = example_lvgl_update_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = amoled_panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Register touch driver to LVGL");
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;//设备种类，触屏设备
    indev_drv.disp = disp;//配置触摸关联的显示屏，使用LVGL注册的显示驱动结构体
    indev_drv.read_cb = lvgl_touch_read_cb;
    indev_drv.user_data = amoled_touch_handle;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    s_lvgl_mutex = xSemaphoreCreateMutex();
    assert(s_lvgl_mutex);

    //AMOLED_refresh();
    ESP_LOGI(TAG, "Create lvgl task");
    AMOLED_render_direction_set(false);
    xTaskCreate(lvgl_task, "task_LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY,NULL);
    AMOLED_console_log(INFORM, false, "ui" ,"CONSOLE_DISPLAY_DISABLE");
}

/* ===================== 设备列表：动态成员行 ===================== */

lv_obj_t * LVGL_list_add_member(int index,
                                const char *member_name,
                                const char *member_type,
                                const char *member_state)
{
    /* 1) 容器必须已存在（需先进入 ListScreen，ui_ListScreen_screen_init 已执行） */
    if (ui_ListContainer == NULL) {
        ESP_LOGW(TAG, "ui_ListContainer is NULL, call after ListScreen init");
        return NULL;
    }

    if (index < 0) {
        ESP_LOGW(TAG, "invalid index: %d", index);
        return NULL;
    }

    /* 2) 创建行面板（样式对齐 ui_ListScreen.c 中的 ui_MemberPanel0） */
    lv_obj_t *memberpanel = lv_obj_create(ui_ListContainer);
    lv_obj_set_width(memberpanel, LVGL_LIST_MEMBER_PANEL_W);
    lv_obj_set_height(memberpanel, LVGL_LIST_MEMBER_PANEL_H);
    lv_obj_clear_flag(memberpanel, LV_OBJ_FLAG_SCROLLABLE);

    /* 3) 由调用方规划纵向顺序：y = 高度 * n */
    lv_obj_set_pos(memberpanel, 0, LVGL_LIST_MEMBER_PANEL_H * index);

    lv_obj_set_style_bg_color(memberpanel, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(memberpanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(memberpanel, lv_color_hex(0x0F0F0F), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(memberpanel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(memberpanel, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 4) MemberName */
    lv_obj_t *lbl_name = lv_label_create(memberpanel);
    lv_obj_set_width(lbl_name, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl_name, LV_SIZE_CONTENT);
    lv_label_set_text(lbl_name, member_name ? member_name : "");
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(0xFDFDFD), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(lbl_name, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 5) MemberType（左下） */
    lv_obj_t *lbl_type = lv_label_create(memberpanel);
    lv_obj_set_width(lbl_type, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl_type, LV_SIZE_CONTENT);
    lv_obj_set_x(lbl_type, 0);
    lv_obj_set_y(lbl_type, 5);
    lv_obj_set_align(lbl_type, LV_ALIGN_BOTTOM_LEFT);
    lv_label_set_text(lbl_type, member_type ? member_type : "");
    lv_obj_set_style_text_color(lbl_type, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(lbl_type, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_type, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* 6) MemberState（右侧居中） */
    lv_obj_t *lbl_state = lv_label_create(memberpanel);
    lv_obj_set_width(lbl_state, LV_SIZE_CONTENT);
    lv_obj_set_height(lbl_state, LV_SIZE_CONTENT);
    lv_obj_set_x(lbl_state, 16);
    lv_obj_set_y(lbl_state, 0);
    lv_obj_set_align(lbl_state, LV_ALIGN_RIGHT_MID);
    lv_label_set_text(lbl_state, member_state ? member_state : "");
    lv_obj_set_style_text_color(lbl_state, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(lbl_state, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(memberpanel, ui_event_MemberPanel, LV_EVENT_ALL, NULL);

    return memberpanel;
}

bool LVGL_list_delete_member(lv_obj_t *member_panel)
{
    if (member_panel == NULL) {
        ESP_LOGW(TAG, "delete member: panel is NULL");
        return false;
    }

    if (ui_ListContainer == NULL) {
        ESP_LOGW(TAG, "delete member: ui_ListContainer is NULL");
        return false;
    }

    if (lv_obj_get_parent(member_panel) != ui_ListContainer) {
        ESP_LOGW(TAG, "delete member: panel is not child of ui_ListContainer");
        return false;
    }

    lv_obj_del(member_panel);

    return true;
}