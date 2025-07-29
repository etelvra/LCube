
#ifndef WIFI_ESP_H_
#define WIFI_ESP_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/

/*!             Header files
 ******************************************************************************/
#include "WIFI.h"

// Configuration definitions: target SSID, LED pin, operation mode buttons, and protocol constants
#define TARGET_SSID "chuer(16)"        // 待攻击的目标WiFi名称 | Target WiFi SSID to attack
#define LED_PIN 2                      // 状态指示灯引脚 | Status LED pin
#define SSID_MAX_LEN 32                // IEEE802.11协议最大SSID长度 | IEEE802.11 maximum SSID length
#define CHANNEL_MIN 1                  // 最小WiFi信道 | Minimum WiFi channel
#define CHANNEL_MAX 14                 // 最大WiFi信道 | Maximum WiFi channel
#define MAC_LEN 6                      // MAC地址字节长度 | MAC address byte length

void WIFI_STA_init(void);

/**
 * @brief 信标洪水攻击任务 | Beacon flooder task
 * @param arg 任务参数（未使用）| Task parameters (unused)
 * 生成随机BSSID和变异SSID，持续发送伪造信标帧 | Generates random BSSID and mutated SSID, continuously sends forged beacon frames
 */
void task_wifi_beacon_spam(void *arg);



/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* WIFI_ESP_H_ */
