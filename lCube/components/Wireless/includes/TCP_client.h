#ifndef TCP_ESP_H_
#define TCP_ESP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ================================================================
 *  通用 TCP 行协议客户端 — 对外 API (App 层只调用这一组函数)
 * ================================================================ */

#define TCP_SERVER_DEFAULT      "bemfa.com"
#define TCP_PORT_DEFAULT        8344
#define TCP_RECV_BUF_LEN        1024
#define TCP_HEARTBEAT_SEC       55

typedef enum {
    TCP_OK = 0,
    TCP_ERR_SOCKET,
    TCP_ERR_CONNECT,
    TCP_ERR_SEND,
    TCP_ERR_PARAM,
    TCP_ERR_MEM,
    TCP_ERR_STATE,
} tcp_err_t;

/** @brief 收到一行完整数据 (已去除 \r\n) */
typedef void (*tcp_on_line_t)(const char *line, int len);

/* ================================================================
 *  协议驱动接口 (底层实现，一种云平台 = 一个驱动实例)
 * ================================================================ */

typedef struct tcp_proto_ops {
    const char *heartbeat_msg;      /* 心跳内容，如 "ping"，NULL 则不发送心跳 */

    /** @brief 连接/重连成功后回调，通常用于发送鉴权/订阅指令 */
    void (*on_connected)(void);

    /**
     * @brief 构建发布消息到 buf (不含 \r\n，由 TCP 层追加)
     * @return 写入 buf 的字节数，<0 表示错误
     */
    int  (*build_publish)(char *buf, int buf_len, const char *topic, const char *msg);
} tcp_proto_ops_t;

/* ================================================================
 *  TCP 客户端配置
 * ================================================================ */

typedef struct {
    char                server[64];
    uint16_t            port;
    const tcp_proto_ops_t *proto_ops;       /* 协议驱动 (如 &bemfa_proto_ops) */
    tcp_on_line_t       on_line;            /* 用户收包回调 */
    char                line_ending[8];     /* 行结束符，默认 "\r\n" */
} tcp_config_t;

/* ---- TCP 公共 API (App 层调用) ---- */

void     TCP_client_init(const tcp_config_t *cfg);
tcp_err_t TCP_client_publish(const char *topic, const char *msg);
int      TCP_client_send(const char *data);          /* 原始发送 */
int      TCP_client_send_line(const char *data);          /* 发送 + 追加 \r\n */
bool     TCP_client_is_connected(void);
void     TCP_client_deinit(void);

/* ================================================================
 *  Bemfa (巴法云) 协议驱动 — 内部模块，App 层不直接调用
 * ================================================================ */

#define BEMFA_UID_MAX_LEN       64
#define BEMFA_TOPIC_MAX_LEN     64
#define BEMFA_MSG_MAX_LEN       512

typedef struct {
    char uid[BEMFA_UID_MAX_LEN];
    char topic[BEMFA_TOPIC_MAX_LEN];
} bemfa_auth_t;

/** @brief 获取巴法云协议驱动实例 (App 初始化时调用一次，获取 ops 指针即可) */
const tcp_proto_ops_t* Bemfa_get_ops(const bemfa_auth_t *auth);

#ifdef __cplusplus
}
#endif

#endif /* TCP_ESP_H_ */
