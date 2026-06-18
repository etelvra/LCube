#include <stdio.h>
#include <inttypes.h>
#include <malloc.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "panel.h"
#include "MIC.h"

static const char *TAG = "mic";
#define FFT_SIZE 1024
#define SAMPLE_RATE       16000
#define AUDIO_BUFFER_SIZE 4096

#define SPECTROGRAM_X  LCD_V_RES
#define SPECTROGRAM_Y  304
//#define render_step (int)(FFT_SIZE/SPECTROGRAM_X/2)

SemaphoreHandle_t adc_mic_mutex = NULL;
QueueHandle_t mic_message_queue = NULL;


void task_adc_mic_listen(void *param)
{
    audio_codec_adc_cfg_t cfg = DEFAULT_AUDIO_CODEC_ADC_MONO_CFG(ADC_CHANNEL_0, 16000);
    cfg.atten = ADC_ATTEN_DB_6;  // 衰减范围
    const audio_codec_data_if_t *adc_if = audio_codec_new_adc_data(&cfg);

    const esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .data_if = adc_if,
    };
    esp_codec_dev_handle_t adcMic_device_handle = esp_codec_dev_new(&codec_dev_cfg);

    const esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE,
        .channel = 1,
        .bits_per_sample = 16,
    };
    esp_codec_dev_open(adcMic_device_handle, &fs);
    ESP_LOGE("dev", "opend");

    adc_mic_mutex = xSemaphoreCreateMutex();
    mic_message_queue = xQueueCreate(4, sizeof(mic_message_t));
    //double buffering 2026/4/7
    uint16_t *audio_buffer_current = (uint16_t*)heap_caps_malloc(sizeof(uint16_t) * AUDIO_BUFFER_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    uint16_t *audio_buffer_next = (uint16_t*)heap_caps_malloc(sizeof(uint16_t) * AUDIO_BUFFER_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);

    mic_message_t mic_message_buffer;
    while(1){
        esp_codec_dev_read(adcMic_device_handle, audio_buffer_current, sizeof(uint16_t) * FFT_SIZE);
        mic_message_buffer.audio_buffer_address = audio_buffer_current;
        mic_message_buffer.buffer_empty = false;
        //Send to the queue (non-blocking)
        if (xQueueSend(mic_message_queue, &mic_message_buffer, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "audio event queue full");
            break;//Task exit mechanism: If there is no data processing task, the task exits.2026/4/9
        }
        audio_buffer_current = audio_buffer_next;//buffer address rotation
        audio_buffer_next = mic_message_buffer.audio_buffer_address;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    esp_codec_dev_close(adcMic_device_handle);
    esp_codec_dev_delete(adcMic_device_handle);
    audio_codec_delete_data_if(adc_if);
    free(audio_buffer_current);
    free(audio_buffer_next);
    vTaskDelete(NULL);
}


void DSP_FFT_process(int16_t *signal_samples, int fft_size, int samples_rate, float *fft_buffer, pixel_t *spectrogram_buffer)
{
    // 1. 将 int16 音频（范围 -32768..32767）转换为浮点复数（实部，虚部=0）
    for (int i = 0; i < fft_size; i++) {
        fft_buffer[2*i]   = (float)(signal_samples[i]-32768) / 32768.0f;   // 归一化到 [-1, 1]
        fft_buffer[2*i+1] = 0.0f;
    }

    // 2. 执行基4 FFT（优化版本）
    dsps_fft4r_fc32(fft_buffer, fft_size);
    // 3. 位反转，得到自然顺序的频谱
    dsps_bit_rev4r_fc32(fft_buffer, fft_size);

    // 4. 计算幅度谱（只取前一半，即 0 ~ sample_rate/2）
    float max_mag = 0.0f;
    int max_idx = 0;
    for (int i = 1; i < fft_size/2; i++) {  // 跳过直流 (i=0)
        float re = fft_buffer[2*i];
        float im = fft_buffer[2*i+1];
        float mag = sqrtf(re*re + im*im);
        //屏幕旋转后向右向上渲染频谱图
        if (i <SPECTROGRAM_X){
        for (int y=0; y < SPECTROGRAM_Y; y++){ //竖屏一行行渲染，横屏一列列看图
            if (y < (int)(mag*100)){
                ((pixel_t *)spectrogram_buffer)[(SPECTROGRAM_X - i)*SPECTROGRAM_Y + (SPECTROGRAM_Y -1 - y)] = PIXEL_WHITE;
            }else{
                ((pixel_t *)spectrogram_buffer)[(SPECTROGRAM_X - i)*SPECTROGRAM_Y + (SPECTROGRAM_Y -1 - y)] = PIXEL_BLACK;
            }
        }}
        if (mag > max_mag) {
            max_mag = mag;
            max_idx = i;
        }
    }
    esp_lcd_panel_draw_bitmap(amoled_panel_handle, 0, 0, SPECTROGRAM_Y, SPECTROGRAM_X, spectrogram_buffer);
    float freq = (float)max_idx * samples_rate / fft_size;
    AMOLED_print_single_line(20, 20*16, false, "frequency: %.2f Hz, magnitude: %.3f", freq, max_mag);
    ESP_LOGI("FFT", "Dominant frequency: %.2f Hz, magnitude: %.3f", freq, max_mag);
}

void task_fft_process(void *param)
{
    mic_message_t mic_message_buffer;
    unsigned int start_b;
    float cycles;

    while (!adc_mic_mutex) {vTaskDelay(pdMS_TO_TICKS(10));}

    dsps_fft4r_init_fc32(NULL, FFT_SIZE);
    float *fft_buffer = (float *)heap_caps_malloc(FFT_SIZE * 2 * sizeof(float), MALLOC_CAP_32BIT | MALLOC_CAP_INTERNAL);
    if (!fft_buffer) {
        ESP_LOGE("FFT", "first Memory allocation failed");
        vTaskDelete(NULL);
    }
    pixel_t * spectrogram_buffer = (pixel_t *)heap_caps_malloc(SPECTROGRAM_X * SPECTROGRAM_Y * sizeof(pixel_t) ,  MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);

    if (xSemaphoreTake(adc_mic_mutex, pdMS_TO_TICKS(50)) && xSemaphoreTake(amoled_panel_mutex, pdMS_TO_TICKS(50)) ) {
        esp_lcd_panel_swap_xy(amoled_panel_handle, 0);
        esp_lcd_panel_mirror(amoled_panel_handle, 0, 0);
        while (1){
            if (xQueueReceive(mic_message_queue, &mic_message_buffer, pdMS_TO_TICKS(3000))) {//wait for notification of the listening task
                if (mic_message_buffer.audio_buffer_address == NULL){
                    break;//Task exit mechanism: If a null pointer is received, the fft_task exits.2026/4/9
                }else{
                    start_b = dsp_get_cpu_cycle_count();
                    DSP_FFT_process( (int16_t*)mic_message_buffer.audio_buffer_address, FFT_SIZE, SAMPLE_RATE,
                                            (float *)fft_buffer, (pixel_t *)spectrogram_buffer);
                    cycles = dsp_get_cpu_cycle_count() - start_b;
                    AMOLED_print_single_line(20, 21*16, false, "%6i CPU cycles for FFT.", (int)cycles);
                    ESP_LOGI(TAG, "%6i CPU cycles for FFT.", (int)cycles);
                }
            }else{
                break;//Task exit mechanism: if there is no notification for a long time, the fft_task exits.2026/4/9
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        xSemaphoreGive(adc_mic_mutex);
        esp_lcd_panel_swap_xy(amoled_panel_handle, 1);
        esp_lcd_panel_mirror(amoled_panel_handle, 0, 1);
        xSemaphoreGive(amoled_panel_mutex);
    } else {
            ESP_LOGW(TAG, "Failed to acquire ADC_MIC mutex or AMOLED_PANEL mutex");
    }
    free(fft_buffer);
    free(spectrogram_buffer);
    vTaskDelete(NULL);
}
