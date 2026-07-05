
#ifndef LIGHT_SLEEP_H_
#define LIGHT_SLEEP_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
#include "panel.h"
#include "AXP2101.h"
#include "freertos/queue.h"
/*!             Header files
 ******************************************************************************/
extern QueueHandle_t lightsleep_event_queue;
extern volatile bool enter_lightsleep;

void task_lightsleep_management(void *param);

/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* LIGHT_SLEEP_H_ */
