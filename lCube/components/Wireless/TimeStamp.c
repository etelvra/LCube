#include <time.h>
#include <string.h>
#include <sys/time.h>
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "panel.h"
//#include "esp_system.h"
//#include "esp_attr.h"
//#include "esp_sleep.h"
//#include "lwip/ip_addr.h"
static const char *TAG = "SNTP";

#define TIME_SNTP_SERVER_0          "ntp.aliyun.com"
#define TIME_SNTP_SERVER_1          "time.windows.com"
#define TIME_SNTP_SERVER_2          "ntp.ntsc.ac.cn"


#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif


static void TIME_SNTP_EVENTfunction_handler(struct timeval *tv)
{//Notification of a time synchronization event
    AMOLED_console_log(INFORM,false,TAG, "TIME synchronization completed");
    printf("%lld       \n",time(NULL));
}

static void print_servers(void)
{
    AMOLED_console_log(INFORM,false,TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
        if (esp_sntp_getservername(i)){
            AMOLED_console_log(INFORM,false,TAG, "server %d: %s", i, esp_sntp_getservername(i));
        } else {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                AMOLED_console_log(INFORM,false,TAG, "server %d: %s", i, buff);
        }
    }
}


void SNTP_obtain_time(void)
{
    //ESP_ERROR_CHECK(nvs_flash_init());
    //ESP_ERROR_CHECK(esp_netif_init());
    //ESP_ERROR_CHECK(esp_event_loop_create_default());

#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
    //This demonstrates configuring more than one server
    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(3,
                                        ESP_SNTP_SERVER_LIST(TIME_SNTP_SERVER_0, TIME_SNTP_SERVER_1,TIME_SNTP_SERVER_2));
#else
    //This is the basic default config with one server and starting the service
    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(TIME_SNTP_SERVER_0);
#endif
    sntp_config.sync_cb = TIME_SNTP_EVENTfunction_handler;     // Note: This is only needed if we want
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_config.smooth_sync = true;
#endif
    esp_netif_sntp_init(&sntp_config);
    print_servers();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 15;
    // wait for time to be set
    while (esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        AMOLED_console_log(INFORM,true,TAG, "Waiting for system time  set... (%d/%d)", retry, retry_count);
    }
//  esp_netif_sntp_deinit();
    sntp_set_sync_interval(CONFIG_LWIP_SNTP_UPDATE_DELAY); //Time synchronization once an hour

    char strftime_buf[64];
    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();
    time(&now);//将系统时间赋值到now  time()函数返回值为系统时间
    localtime_r(&now, &timeinfo);//将时间now分解为日历时间并赋值到timeinfo
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    AMOLED_console_log(INFORM,false,TAG, "The current time is: %s", strftime_buf);
}

