
#ifndef AMOLED_PANEL_H_
#define AMOLED_PANEL_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/*!             Header files
 ******************************************************************************/
void init_lcd(void);
void AMOLED_TP_init(void);
void touch_read_task(void *arg);

/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* AMOLED_PANEL_H_ */
