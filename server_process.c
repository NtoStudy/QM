/*
 * server_process.c  — 实验2：多进程并发远程计算器服务器
 * 作者：牛文俊
 * 学号：230207224
 * 完成日期：2026-06-12
 * 功能：基于fork实现多进程并发服务端，支持同时处理多个客户端连接
 * 编译命令: gcc -o server_process server_process.c calculate.c
 * 运行命令: ./server_process [端口]     不指定端口默认使用8888
 * 程序说明:
 *   1. 父进程调用accept监听客户端连接，通过fork创建子进程处理业务
 *   2. 子进程完成数据收发与计算任务后正常退出
 *   3. 父进程使用waitpid非阻塞回收子进程，避免产生僵尸进程
 *   4. 父子进程分别关闭无用文件描述符，防止系统资源泄漏
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#include "calculate.h"   // 引入四则运算计算功能头文件

#define DEFAULT_PORT 8888  // 服务端默认监听端口
#define MAX_LINE     4096  // 数据收发缓冲区最大长度

int main(int argc, char *argv[]) {
    int    socket_fd;         // 服务端监听套接字文件描述符
    int    connected_fd;     // 客户端已连接套接字文件描述符
    struct sockaddr_in servaddr;  // 服务端地址信息结构体
    int    port = DEFAULT_PORT;   // 监听端口号

    // 解析命令行参数，自定义监听端口
    if (argc >= 2) port = atoi(argv[1]);

    /* ---- 1. 创建IPv4协议下的TCP流式套接字 ---- */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create socket error: %s (errno: %d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    /* ---- 2. 绑定本机IP与指定端口 ---- */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;                // 使用IPv4协议簇
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);      // 绑定本机所有网卡地址
    servaddr.sin_port        = htons(port);            // 端口转换为网络字节序

    if (bind(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s (errno: %d)\n", strerror(errno), errno);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* ---- 3. 开启端口监听，设置连接队列上限 ---- */
    if (listen(socket_fd, 10) == -1) {
        printf("listen socket error: %s (errno: %d)\n", strerror(errno), errno);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("【实验2 — 多进程】服务器启动，监听端口 %d ...\n", port);

    /* ---- 4. 循环接收连接、创建子进程实现并发处理 ---- */
    while (1) {
        // 阻塞等待客户端连接，生成通信套接字
        if ((connected_fd = accept(socket_fd, NULL, NULL)) == -1) {
            printf("accept socket error: %s (errno: %d)\n", strerror(errno), errno);
            continue;
        }

        // 创建子进程，实现并发处理
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(connected_fd);
            continue;
        }

        if (pid == 0) {
            /* ========== 子进程：负责与客户端通信、计算 ========== */
            close(socket_fd);  // 子进程无需监听，关闭监听套接字

            char buff[MAX_LINE];
            memset(buff, 0, sizeof(buff));
            // 接收客户端发送的运算表达式
            int n = recv(connected_fd, buff, sizeof(buff) - 1, 0);
            if (n > 0) {
                buff[n] = '\0';
                printf("[子进程 %d] 收到: %s\n", getpid(), buff);

                // 调用计算函数执行四则运算
                char res[64];
                calculate(buff, res);
                printf("[子进程 %d] 结果: %s\n", getpid(), res);

                // 向客户端返回计算结果
                send(connected_fd, res, strlen(res), 0);
            }

            close(connected_fd);  // 关闭通信套接字
            exit(EXIT_SUCCESS);   // 子进程正常退出
        } else {
            /* ========== 父进程：持续监听新连接、回收子进程 ========== */
            close(connected_fd); // 父进程不参与通信，关闭已连接套接字
            // 非阻塞方式回收已退出子进程，防止产生僵尸进程
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    close(socket_fd);
    return 0;
}
