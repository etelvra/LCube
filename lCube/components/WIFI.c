#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_random.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "WIFI.h"


static EventGroupHandle_t s_wifi_event_group;
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define WIFI_STA_DEFAULT_ID          "TP-LINK_EE0A"
#define WIFI_STA_DEFAULT_PASSWORD    "20020911#"
static int s_retry_num = 0;


static void WIFI_EVENTfunction_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id){
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_retry_num < 5) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI("wifi station", "retry to connect to the AP");
                    } else {
                        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    }
                    ESP_LOGI("wifi station","connect to the AP fail");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI("wifi station","connect to the AP success");
                break;
            default:break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("wifi station", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}



void WIFI_STA_init(void)
{
    s_wifi_event_group = xEventGroupCreate();
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WIFI_EVENTfunction_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP, &WIFI_EVENTfunction_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid     = WIFI_STA_DEFAULT_ID,
            .password = WIFI_STA_DEFAULT_PASSWORD,

            .threshold.authmode = WIFI_AUTH_WPA2_PSK,//加密模式
            .sae_pwe_h2e        = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",                //Password identifier for H2E
            .pmf_cfg.capable    = true,             //启用保护管理帧
            .pmf_cfg.required   = false,            //是否只连接具有保护管理帧的设备
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI("wifi station", "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdFALSE,pdFALSE,portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI("wifi station", "connected to ap SSID:%s password:%s",WIFI_STA_DEFAULT_ID , WIFI_STA_DEFAULT_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI("wifi station", "Failed to connect to SSID:%s, password:%s",WIFI_STA_DEFAULT_ID, WIFI_STA_DEFAULT_PASSWORD);
    } else {
        ESP_LOGE("wifi station", "UNEXPECTED EVENT");
    }
}

// Force disable 802.11 frame validation (allows sending malformed frames) from https://github.com/risinek/esp32-wifi-penetration-tool/blob/master/components/wsl_bypasser/wsl_bypasser.c
//int ieee80211_raw_frame_sanity_check(int32_t arg, int32_t arg2, int32_t arg3){return 0;}

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
  /* 38 - 69 */ 0x20, 0x20, 0x20, 0x20,
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
