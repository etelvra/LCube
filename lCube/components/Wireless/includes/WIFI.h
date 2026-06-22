
#ifndef WIFI_ESP_H_
#define WIFI_ESP_H_

/*! CPP guard */
#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************/
#include "WIFI.h"
#include "TimeStamp.h"
#include "panel.h"
#include "lvgl_display.h"
/*!             Header files
 ******************************************************************************/

#define MAX_SCAN_NUM    20
#define SSID_MAX_LEN       32
#define CHANNEL_MIN        1
#define CHANNEL_MAX        14
#define MAC_LEN            6

typedef enum {
    WIFI_CMD_SCAN = 0,
    WIFI_CMD_MONITOR_START,
    WIFI_CMD_MONITOR_STOP,
    WIFI_CMD_CONNECT,
    WIFI_CMD_DISCONNECT,
    WIFI_CMD_DEAUTH_ATTACK,
    WIFI_CMD_BEACON_SPAM_START,
    WIFI_CMD_BEACON_SPAM_STOP,
    WIFI_CMD_GET_STATUS,
} wifi_cmd_type_t;

typedef enum {
    WIFI_RESULT_OK = 0,
    WIFI_RESULT_ERR_UNKNOWN_CMD,
    WIFI_RESULT_ERR_STATE,
    WIFI_RESULT_ERR_TIMEOUT,
    WIFI_RESULT_ERR_PARAM,
} wifi_result_code_t;

/** @brief SCAN 命令参数 */
typedef struct {
    char     target_ssid[SSID_MAX_LEN];
    uint8_t  target_channel;
    bool     show_hidden;
    uint32_t scan_time_min_ms;
    uint32_t scan_time_max_ms;
} wifi_cmd_scan_params_t;

/** @brief DEAUTH ATTACK 命令参数 */
typedef struct {
    uint8_t  ap_bssid[MAC_LEN];
    uint8_t  sta_bssid[MAC_LEN];
    uint16_t reason_code;
    uint16_t repeat_count;    /* 0 = 无限循环直到停止命令 */
    uint16_t interval_ms;
} wifi_cmd_deauth_params_t;

/** @brief BEACON SPAM 命令参数 */
typedef struct {
    char     base_ssid[SSID_MAX_LEN];
    uint8_t  target_channel;  /* 0 = 随机信道 */
    uint16_t interval_ms;
} wifi_cmd_beacon_spam_params_t;

/** @brief MONITOR 命令参数 */
typedef struct {
    uint8_t  ap_bssid[MAC_LEN];
    uint8_t  target_channel;
} wifi_cmd_monitor_params_t;

/** @brief CONNECT 命令参数 */
typedef struct {
    char     ssid[32];
    char     password[64];
    uint8_t  bssid[MAC_LEN];  /* 全 0 = 不指定 BSSID */
} wifi_cmd_connect_params_t;

/******************************************************************************/
/*!                 命令消息 (队列元素)
 ******************************************************************************/
typedef struct wifi_task_queue_message{
    wifi_cmd_type_t cmd_type;
    uint32_t        request_id;
    TaskHandle_t    reply_task;
    union {
        wifi_cmd_scan_params_t          scan;
        wifi_cmd_deauth_params_t        deauth;
        wifi_cmd_beacon_spam_params_t   beacon_spam;
        wifi_cmd_monitor_params_t       monitor;
        wifi_cmd_connect_params_t       connect;
    } params;
} wifi_task_queue_message_t;

typedef void (*wifi_cmd_handler_t)(const wifi_task_queue_message_t *msg);

typedef struct {
    wifi_cmd_type_t     cmd;
    wifi_cmd_handler_t  handler;
} wifi_cmd_dispatch_entry_t;

/******************************************************************************/
/*!                 状态信息 (GET_STATUS 响应数据)
 ******************************************************************************/
typedef struct {
    bool    is_monitoring;
    bool    is_attacking;
    bool    is_connected;
    char    current_ssid[33];
    int8_t  current_rssi;
} wifi_status_info_t;

/******************************************************************************/
/*!                 响应体 (通过 TaskNotify 回传)
 ******************************************************************************/
typedef struct {
    wifi_cmd_type_t    cmd_type;
    wifi_result_code_t result;
    uint32_t           request_id;
    union {
        wifi_status_info_t status;
    } data;
} wifi_task_response_t;


void WIFI_init(void);
void WIFI_STA_deinit(void);

wifi_result_code_t WIFI_send_cmd(wifi_cmd_type_t cmd, const void *params, TaskHandle_t reply_task, uint32_t req_id);

/**
 * 802.11 管理帧公共头部 (24 字节)
 * 参考 IEEE Std 802.11-2020 第 9.3.3.2 节
 */
typedef struct ieee80211_mgmt_header {
    /*  0 - 1  */ uint8_t frame_control[2];   // 帧控制字段 (协议版本、类型、子类型、标志位)
    /*  2 - 3  */ uint8_t duration[2];        // 持续时间/ID (用于 NAV 设置)
    /*  4 - 9  */ uint8_t da[6];              // 地址1: 目的地址 (Destination Address)
    /* 10 - 15 */ uint8_t sa[6];              // 地址2: 源地址 (Source Address)
    /* 16 - 21 */ uint8_t bssid[6];           // 地址3: BSSID (基本服务集标识符)
    /* 22 - 23 */ uint8_t seq_ctrl[2];        // 序列控制字段 (片段号 + 序列号)
} __attribute__((packed)) ieee80211_mgmt_hdr_t;

/**
 * @brief Beacon 帧的固定参数部分 (紧接在公共头部之后)
 */
typedef struct ieee80211_beacon_fixed {
    /* 24 -31 */ uint64_t timestamp;       // Timestamp(ms)
    /* 32 -33 */ uint16_t beacon_interval; // Beacon Interval信标间隔 (TU, 1 TU = 1024 μs)0x64, 0x00 => every 100ms - 0xe8, 0x03 => every 1s
    /* 34 -35 */ uint16_t capability;      // capabilities Tnformation能力信息
    /* 36 -   */ // 之后是 Tagged parameters (可变长度)
} __attribute__((packed)) ieee80211_beacon_fixed_t;

/**
 * @brief 完整的 Beacon 帧结构 (含公共头部 + 固定参数)
 * @note  实际使用时需在结构体后添加可变长度标签参数
 */
typedef struct {
    ieee80211_mgmt_hdr_t   header;         // 公共头部
    ieee80211_beacon_fixed_t fixed;        // 固定参数
    // uint8_t tagged_params[];            // 标签参数(柔性数组)
} __attribute__((packed)) ieee80211_beacon_frame_t;


//void WIFI_scan_ap(void);
void WIFI_deauth_attack(const uint8_t *ap_bssid, const uint8_t *sta_bssid, uint16_t reason_code);



/******************************************************************************/
/*! @name       C++ Guard Macros                                      */
/******************************************************************************/
#ifdef __cplusplus
}
#endif /* End of CPP guard */

#endif /* WIFI_ESP_H_ */
