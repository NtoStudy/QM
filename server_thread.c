/*
 * server_thread.c  — 实验2：多线程并发远程计算器服务器
 * 作者：牛文俊
 * 学号：230207224
 * 完成日期：2026-06-12
 * 功能：基于pthread实现多线程并发服务端，为每个客户端单独创建子线程处理请求
 * 编译命令: gcc -o server_thread server_thread.c calculate.c -pthread
 * 运行命令: ./server_thread [端口]     不指定端口默认使用8888
 * 程序说明:
 *   1. 主线程负责监听并接收客户端连接，每建立一个连接就创建子线程
 *   2. 使用malloc动态分配内存存储套接字描述符，解决线程数据竞态问题
 *   3. 线程设置为分离态，结束后自动回收资源
 *   4. 编译必须添加 -pthread 参数链接线程库
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "calculate.h"   // 引入四则运算计算函数头文件

#define DEFAULT_PORT 8888  // 服务端默认监听端口
#define MAX_LINE     4096  // 数据收发缓冲区最大长度

/**
 * @brief 线程处理函数，负责与单个客户端通信、计算并返回结果
 * @param client_socket_ptr 指向已连接套接字描述符的指针
 */
void *handle_client(void *client_socket_ptr) {
    /* 将线程设为分离态，线程退出后系统自动释放资源 */
    pthread_detach(pthread_self());

    // 取出套接字描述符并释放动态内存
    int client_fd = *(int *)client_socket_ptr;
    free(client_socket_ptr);

    char buff[MAX_LINE];
    memset(buff, 0, sizeof(buff));
    // 接收客户端发送的运算表达式
    int n = recv(client_fd, buff, sizeof(buff) - 1, 0);
    if (n > 0) {
        buff[n] = '\0';
        printf("[线程 %lu] 收到: %s\n", pthread_self(), buff);

        // 调用计算函数执行四则运算
        char res[64];
        calculate(buff, res);
        printf("[线程 %lu] 结果: %s\n", pthread_self(), res);

        // 向客户端发送计算结果
        send(client_fd, res, strlen(res), 0);
    }

    close(client_fd);       // 关闭通信套接字
    pthread_exit(NULL);     // 子线程正常退出
}

int main(int argc, char *argv[]) {
    int    socket_fd;               // 服务端监听套接字文件描述符
    struct sockaddr_in servaddr;    // 服务端地址信息结构体
    int    port = DEFAULT_PORT;     // 监听端口号

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
    servaddr.sin_port        = htons(port);            // 端口转为网络字节序

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

    printf("【实验2 — 多线程】服务器启动，监听端口 %d ...\n", port);

    /* ---- 4. 循环接收客户端连接，动态创建子线程处理请求 ---- */
    while (1) {
        // 动态分配堆内存存储套接字描述符，避免主线程覆盖变量造成竞态条件
        int *connected_fdp = (int *)malloc(sizeof(int));
        if (connected_fdp == NULL) {
            perror("malloc");
            continue;
        }

        // 阻塞等待客户端连接
        *connected_fdp = accept(socket_fd, NULL, NULL);
        if (*connected_fdp == -1) {
            printf("accept error: %s (errno: %d)\n", strerror(errno), errno);
            free(connected_fdp);
            continue;
        }

        // 创建子线程处理当前客户端请求
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, (void *)connected_fdp) != 0) {
            perror("pthread_create");
            close(*connected_fdp);
            free(connected_fdp);
        }
    }

    close(socket_fd);
    return 0;
}
