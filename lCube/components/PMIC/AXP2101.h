
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
#include "driver/i2c_master.h"

extern i2c_master_bus_handle_t i2c_handle;
esp_err_t i2c_init(void);
esp_err_t i2c_deinit(void);
void AXP2101_init(void);
void AXP2101_IRQStatus_clear(void);
esp_err_t AXP2101_bat_percentage(void);
void check_axp2101_status(uint8_t *status_data);


/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* PMIC_AXP2101_H_ */
