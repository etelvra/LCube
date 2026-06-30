
#ifndef CUBE_OTA_H_
#define CUBE_OTA_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/*!             Header files
 ******************************************************************************/

void OTA_verify_version(void);
void task_ota(void *param);
void OTA_request_cancel(void);


/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* CUBE_OTA_H_ */
