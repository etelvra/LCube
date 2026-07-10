#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "TCP_client.h"

static const char *TAG = "TCP";

/* ================================================================
 *  TCP 层 — 内部状态
 * ================================================================ */

static tcp_config_t      s_cfg;
static int               s_sock = -1;
static TimerHandle_t     s_heartbeat_timer;
static TaskHandle_t      s_recv_task;
static bool              s_running;
static SemaphoreHandle_t s_mutex;

/* ---- 重连退避 ---- */
#define RECONNECT_DELAY_MIN_MS  1000
#define RECONNECT_DELAY_MAX_MS  60000
static int s_reconnect_delay_ms = RECONNECT_DELAY_MIN_MS;

/* ================================================================
 *  TCP 层 — 内部辅助
 * ================================================================ */

/** @brief 安全读取 s_sock，避免在 send/recv 期间持有锁 */
static int get_sock(void)
{
    int fd;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    fd = s_sock;
    xSemaphoreGive(s_mutex);
    return fd;
}

/** @brief 安全设置 s_sock（重连成功后调用，会先关闭旧 fd） */
static void set_sock(int fd)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_sock >= 0) close(s_sock);
    s_sock = fd;
    xSemaphoreGive(s_mutex);
}

/* ================================================================
 *  TCP 层 — 连接管理
 * ================================================================ */

static int tcp_connect(void)
{
    struct addrinfo hints = {0};
    struct addrinfo *res;

    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", s_cfg.port);

    if (getaddrinfo(s_cfg.server, port_str, &hints, &res) != 0) {
        ESP_LOGE(TAG, "DNS resolve failed: %s", s_cfg.server);
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        ESP_LOGE(TAG, "socket() failed");
        return -1;
    }

    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
        freeaddrinfo(res);
        close(fd);
        ESP_LOGE(TAG, "connect() to %s:%u failed", s_cfg.server, s_cfg.port);
        return -1;
    }

    freeaddrinfo(res);
    ESP_LOGI(TAG, "Connected to %s:%u", s_cfg.server, s_cfg.port);
    return fd;
}

static void tcp_disconnect(void)
{
    /* 先停心跳，避免在已关闭的 socket 上发送 */
    if (s_heartbeat_timer) {
        xTimerStop(s_heartbeat_timer, 0);
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
    xSemaphoreGive(s_mutex);
}

/* ================================================================
 *  TCP 层 — 心跳
 * ================================================================ */

static void tcp_heartbeat_cb(TimerHandle_t t)
{
    int fd = get_sock();
    if (fd < 0) return;
    if (s_cfg.proto_ops && s_cfg.proto_ops->heartbeat_msg) {
        TCP_client_send_line(s_cfg.proto_ops->heartbeat_msg);
    }
}

/* ================================================================
 *  TCP 层 — 接收任务
 * ================================================================ */

static void tcp_recv_task(void *param)
{
    char buf[TCP_RECV_BUF_LEN];
    char accum[TCP_RECV_BUF_LEN * 2];
    const int line_end_len = (int)strlen(s_cfg.line_ending);

    memset(accum, 0, sizeof(accum));

    while (s_running) {
        int fd = get_sock();
        if (fd < 0) {
            /* 指数退避重连 */
            ESP_LOGI(TAG, "Reconnecting in %d ms...", s_reconnect_delay_ms);
            vTaskDelay(pdMS_TO_TICKS(s_reconnect_delay_ms));

            if (!s_running) break;

            int new_fd = tcp_connect();
            if (new_fd >= 0) {
                set_sock(new_fd);
                s_reconnect_delay_ms = RECONNECT_DELAY_MIN_MS;

                if (s_cfg.proto_ops && s_cfg.proto_ops->on_connected) {
                    s_cfg.proto_ops->on_connected();
                }

                if (s_heartbeat_timer) {
                    xTimerStart(s_heartbeat_timer, 0);
                }
            } else {
                s_reconnect_delay_ms *= 2;
                if (s_reconnect_delay_ms > RECONNECT_DELAY_MAX_MS) {
                    s_reconnect_delay_ms = RECONNECT_DELAY_MAX_MS;
                }
            }
            continue;
        }

        int n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            ESP_LOGW(TAG, "Recv error (n=%d), reconnecting...", n);
            tcp_disconnect();
            continue;
        }

        buf[n] = '\0';
        strncat(accum, buf, sizeof(accum) - strlen(accum) - 1);

        char *start = accum;
        char *end;
        while ((end = strstr(start, s_cfg.line_ending)) != NULL) {
            *end = '\0';
            if (s_cfg.on_line && strlen(start) > 0) {
                s_cfg.on_line(start, end - start);
            }
            start = end + line_end_len;
        }

        if (start > accum) {
            memmove(accum, start, strlen(start) + 1);
        }
        /* 清除已移动数据后的残留 */
        size_t remaining = strlen(accum);
        if (remaining + 1 < sizeof(accum)) {
            memset(accum + remaining + 1, 0, sizeof(accum) - remaining - 1);
        }
    }
    vTaskDelete(NULL);
}

/* ================================================================
 *  TCP 层 — 公开 API (App 层直接调用)
 * ================================================================ */

void TCP_client_init(const tcp_config_t *cfg)
{
    if (s_running) return;

    s_mutex = xSemaphoreCreateMutex();

    memcpy(&s_cfg, cfg, sizeof(tcp_config_t));
    if (s_cfg.server[0] == '\0')       strcpy(s_cfg.server, TCP_SERVER_DEFAULT);
    if (s_cfg.port == 0)               s_cfg.port = TCP_PORT_DEFAULT;
    if (s_cfg.line_ending[0] == '\0')  strcpy(s_cfg.line_ending, "\r\n");
    s_running = true;

    s_reconnect_delay_ms = RECONNECT_DELAY_MIN_MS;

    int fd = tcp_connect();
    set_sock(fd);
    if (fd >= 0) {
        if (s_cfg.proto_ops && s_cfg.proto_ops->on_connected) {
            s_cfg.proto_ops->on_connected();
        }
    } else {
        ESP_LOGW(TAG, "Initial connect failed, will retry in recv task");
    }

    xTaskCreate(tcp_recv_task, "task_tcp_recv", 8192, NULL, 5, &s_recv_task);

    s_heartbeat_timer = xTimerCreate("tcp_heartbeat",
        pdMS_TO_TICKS(TCP_HEARTBEAT_SEC * 1000), pdTRUE, NULL, tcp_heartbeat_cb);
    if (fd >= 0) {
        xTimerStart(s_heartbeat_timer, 0);
    }

    ESP_LOGI(TAG, "TCP client init done (%s:%u)", s_cfg.server, s_cfg.port);
}

tcp_err_t TCP_client_publish(const char *topic, const char *msg)
{
    int fd = get_sock();
    if (fd < 0)                                         return TCP_ERR_STATE;
    if (!s_cfg.proto_ops || !s_cfg.proto_ops->build_publish) return TCP_ERR_PARAM;
    if (!topic || !msg)                                 return TCP_ERR_PARAM;

    char buf[BEMFA_MSG_MAX_LEN + 256];
    int len = s_cfg.proto_ops->build_publish(buf, sizeof(buf), topic, msg);
    if (len < 0) return TCP_ERR_PARAM;

    return TCP_client_send_line(buf) > 0 ? TCP_OK : TCP_ERR_SEND;
}

int TCP_client_send(const char *data)
{
    int fd = get_sock();
    if (fd < 0 || !data) return -1;

    int len = strlen(data);
    int total = 0;
    while (total < len) {
        int sent = send(fd, data + total, len - total, 0);
        if (sent < 0) {
            ESP_LOGE(TAG, "send() failed (total=%d/%d)", total, len);
            return -1;
        }
        total += sent;
    }
    return total;
}

int TCP_client_send_line(const char *data)
{
    int fd = get_sock();
    if (fd < 0 || !data) return -1;

    char buf[TCP_RECV_BUF_LEN];
    int len = snprintf(buf, sizeof(buf), "%s%s", data, s_cfg.line_ending);

    int total = 0;
    while (total < len) {
        int sent = send(fd, buf + total, len - total, 0);
        if (sent < 0) {
            ESP_LOGE(TAG, "send_line() failed (total=%d/%d)", total, len);
            return -1;
        }
        total += sent;
    }
    return total;
}

bool TCP_client_is_connected(void)
{
    return (get_sock() >= 0);
}

void TCP_client_deinit(void)
{
    s_running = false;

    /* 先断开连接（停心跳 + 关 socket），解除 recv 阻塞 */
    tcp_disconnect();

    /* 强制删除接收任务 */
    if (s_recv_task) {
        vTaskDelete(s_recv_task);
        s_recv_task = NULL;
    }

    if (s_heartbeat_timer) {
        xTimerDelete(s_heartbeat_timer, 0);
        s_heartbeat_timer = NULL;
    }

    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }

    ESP_LOGI(TAG, "TCP client deinit");
}

/* ================================================================
 *  Bemfa 协议驱动 — 内部实现，通过 tcp_proto_ops 注册到 TCP 层
 *  App 层不直接调用这里的任何函数
 * ================================================================ */

static bemfa_auth_t s_bemfa;

static void bemfa_on_connected(void)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cmd=1&uid=%s&topic=%s",
             s_bemfa.uid, s_bemfa.topic);
    TCP_client_send_line(cmd);
    ESP_LOGI(TAG, "Bemfa: subscribed topic=%s", s_bemfa.topic);
}

static int bemfa_build_publish(char *buf, int buf_len,
                               const char *topic, const char *msg)
{
    return snprintf(buf, buf_len, "cmd=2&uid=%s&topic=%s&msg=%s",
                    s_bemfa.uid, topic, msg);
}

static const tcp_proto_ops_t s_bemfa_ops = {
    .heartbeat_msg = "ping",
    .on_connected  = bemfa_on_connected,
    .build_publish = bemfa_build_publish,
};

const tcp_proto_ops_t* Bemfa_get_ops(const bemfa_auth_t *auth)
{
    memcpy(&s_bemfa, auth, sizeof(bemfa_auth_t));
    return &s_bemfa_ops;
}
