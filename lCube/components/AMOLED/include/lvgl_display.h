#ifndef LVGL_DISPLAY_H_
#define LVGL_DISPLAY_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
#include "panel.h"
#include "light_sleep.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include "ui.h"
/*!             Header files
 ******************************************************************************/
#define LVGL_TICK_PERIOD_MS    10
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 2
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     12

void AMOLED_LVGL_init(void);

bool lvgl_lock(int timeout_ms);
void lvgl_unlock(void);

#define LVGL_LIST_MEMBER_PANEL_H   80
#define LVGL_LIST_MEMBER_PANEL_W   SCREEN_WIDTH

lv_obj_t * LVGL_list_add_member(int index, const char *member_name, const char *member_type, const char *member_state);
bool       LVGL_list_delete_member(lv_obj_t *member_panel);


/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* LVGL_DISPLAY_H_ */
