/*
 * server.c  — 实验1：单线程远程计算器服务器
 * 作者：牛文俊
 * 学号：230207224
 * 完成日期：2026-06-12
 * 功能：基于TCP实现串行服务端，单次仅处理一个客户端连接，完成四则表达式计算
 * 编译命令: gcc -o server server.c calculate.c
 * 运行命令: ./server [端口]    不指定端口则默认使用8888
 * 程序说明:
 *   1. 采用单线程串行模型，同一时刻仅能服务一个客户端
 *   2. 处理完当前客户端请求后，才会继续接收下一个连接
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>

#include "calculate.h"   // 引入四则运算计算函数头文件

#define DEFAULT_PORT 8888  // 服务端默认监听端口
#define MAX_LINE     4096  // 数据收发缓冲区最大长度

int main(int argc, char *argv[]) {
    int    socket_fd;         // 服务端监听套接字文件描述符
    int    connected_fd;     // 客户端已连接套接字文件描述符
    struct sockaddr_in servaddr;  // 服务端地址结构体
    char   buff[MAX_LINE];    // 数据接收缓冲区
    int    port = DEFAULT_PORT;  // 监听端口，默认8888

    // 解析命令行参数，自定义监听端口
    if (argc >= 2) port = atoi(argv[1]);

    /* ---- 1. 创建IPv4协议下的TCP流式套接字 ---- */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create socket error: %s (errno: %d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    /* ---- 2. 绑定IP地址与端口 ---- */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;                // 使用IPv4协议
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);      // 绑定本机所有网卡IP
    servaddr.sin_port        = htons(port);            // 端口转为网络字节序

    if (bind(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s (errno: %d)\n", strerror(errno), errno);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* ---- 3. 开启端口监听，设置连接等待队列长度 ---- */
    if (listen(socket_fd, 10) == -1) {
        printf("listen socket error: %s (errno: %d)\n", strerror(errno), errno);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("【实验1】服务器启动，监听端口 %d ...\n", port);

    /* ---- 4. 循环阻塞等待并处理客户端连接 ---- */
    while (1) {
        // 阻塞等待客户端连接，生成专属通信套接字
        if ((connected_fd = accept(socket_fd, NULL, NULL)) == -1) {
            printf("accept socket error: %s (errno: %d)\n", strerror(errno), errno);
            continue;
        }
        printf("客户端已连接\n");

        /* 接收客户端发送的运算表达式 */
        memset(buff, 0, sizeof(buff));
        int n = recv(connected_fd, buff, sizeof(buff) - 1, 0);
        if (n <= 0) {
            printf("recv 失败或客户端断开\n");
            close(connected_fd);
            continue;
        }
        buff[n] = '\0';  // 补充字符串结束符
        printf("收到表达式: %s\n", buff);

        /* 调用计算函数，解析并运算表达式 */
        char res[64];
        calculate(buff, res);
        printf("计算结果: %s\n", res);

        /* 将计算结果发送回客户端 */
        send(connected_fd, res, strlen(res), 0);

        // 单次通信完成，关闭已连接套接字
        close(connected_fd);
        printf("客户端连接已关闭\n\n");
    }

    close(socket_fd);
    return 0;
}
