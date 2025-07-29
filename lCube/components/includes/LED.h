
#ifndef LED_LIGHT_H_
#define LED_LIGHT_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/*!             Header files
 ******************************************************************************/
#include "LED.h"

void ledc_configer(void );

void task_led_indicator(void *param);


/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* LED_LIGHT_H_ */
