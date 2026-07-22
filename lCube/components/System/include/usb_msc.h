#ifndef USB_MSC_H_
#define USB_MSC_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 USB 大容量存储设备
 * @note 必须在 FatFS_init() 之后调用
 * @return ESP_OK 成功，否则失败
 */
esp_err_t usb_msc_init(void);

/**
 * @brief 反初始化 MSC 设备
 */
esp_err_t usb_msc_deinit(void);

/**
 * @brief 检查 USB 是否已连接（即主机已枚举 MSC）
 */
bool usb_msc_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_MSC_H_ */