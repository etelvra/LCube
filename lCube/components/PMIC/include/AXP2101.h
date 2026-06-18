
#ifndef PMIC_AXP2101_H_
#define PMIC_AXP2101_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/*!             Header files
 ******************************************************************************/
#include "AXP2101.h"
#include "i2c_bus.h"

extern i2c_bus_handle_t i2c_bus_handle;
extern bool i2c_bus_initialized;
esp_err_t i2c_bus_init(void);
void PMIC_init(void);
uint8_t AXP2101_bat_percentage(void);
void AXP2101_amoled_turn_off(void);
void AXP2101_sys_shutdown(void);

/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* PMIC_AXP2101_H_ */
