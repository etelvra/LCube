#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "TCP_client.h"

static void on_server_line(const char *line, int len)
{
    if (strncmp(line, "on", len) == 0) {
        // 开灯
    } else if (strncmp(line, "off", len) == 0) {
        // 关灯
    }
}


void tcp_demo(void)
{
    /* 初始化 */
    bemfa_auth_t auth = { .uid = "uid", .topic = "lCubeTcp" };
    tcp_config_t cfg = {
        .proto_ops = Bemfa_get_ops(&auth),
        .on_line   = on_server_line,
    };
    TCP_client_init(&cfg);

    /* 运行时 */
    if (TCP_client_is_connected()) {
        TCP_client_publish("lCubeTcp", "hello from esp32");
    }

    TCP_client_send_line("cmd=3&uid=uid&topic=lCubeTcp&msg=custom");

    TCP_client_deinit();
}