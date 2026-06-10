/*
 * server.c  — 实验1：单线程远程计算器服务器
 *
 * 编译: gcc -o server server.c calculate.c
 * 运行: ./server [端口]          （默认 8888）
 *
 * 说明:
 *   - 同一时刻只能处理一个客户端连接
 *   - 处理完当前客户端后才 accept 下一个
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "calculate.h"

#define DEFAULT_PORT 8888
#define MAX_LINE     4096

int main(int argc, char *argv[]) {
    int    socket_fd, connected_fd;
    struct sockaddr_in servaddr;
    char   buff[MAX_LINE];
    int    port = DEFAULT_PORT;

    if (argc >= 2) port = atoi(argv[1]);

    /* ---- 1. 创建套接字 ---- */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create socket error: %s (errno: %d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    /* ---- 2. 绑定地址 ---- */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);

    if (bind(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s (errno: %d)\n", strerror(errno), errno);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* ---- 3. 监听 ---- */
    if (listen(socket_fd, 10) == -1) {
        printf("listen socket error: %s (errno: %d)\n", strerror(errno), errno);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("【实验1】服务器启动，监听端口 %d ...\n", port);

    /* ---- 4. 循环接受客户端连接 ---- */
    while (1) {
        if ((connected_fd = accept(socket_fd, NULL, NULL)) == -1) {
            printf("accept socket error: %s (errno: %d)\n", strerror(errno), errno);
            continue;
        }
        printf("客户端已连接\n");

        /* 接收计算表达式 */
        memset(buff, 0, sizeof(buff));
        int n = recv(connected_fd, buff, sizeof(buff) - 1, 0);
        if (n <= 0) {
            printf("recv 失败或客户端断开\n");
            close(connected_fd);
            continue;
        }
        buff[n] = '\0';
        printf("收到表达式: %s\n", buff);

        /* 执行计算 */
        char res[64];
        calculate(buff, res);
        printf("计算结果: %s\n", res);

        /* 发送结果 */
        send(connected_fd, res, strlen(res), 0);

        close(connected_fd);
        printf("客户端连接已关闭\n\n");
    }

    close(socket_fd);
    return 0;
}
