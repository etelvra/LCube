
#ifndef MICROPHONE_H_
#define MICROPHONE_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
#include "adc_mic.h"
#include "esp_codec_dev.h"
#include "dsps_fft4r.h"
#include "esp_dsp.h"
/*!             Header files
 ******************************************************************************/
typedef struct {                        //Send structure to the queue
    uint16_t *audio_buffer_address;
    bool buffer_empty;
} mic_message_t;

void task_adc_mic_listen(void *param);
void task_fft_process(void *param);

/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* MICROPHONE_H_ */
