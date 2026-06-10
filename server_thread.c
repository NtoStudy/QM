/*
 * server_thread.c  — 实验2：多线程并发远程计算器服务器
 *
 * 编译: gcc -o server_thread server_thread.c calculate.c -pthread
 * 运行: ./server_thread [端口]     （默认 8888）
 *
 * 说明:
 *   - 主线程 accept 后为每个客户端创建一个子线程
 *   - connected_fd 使用 malloc 动态分配，避免竞态条件
 *   - 编译时需要 -pthread 链接 pthread 库
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "calculate.h"

#define DEFAULT_PORT 8888
#define MAX_LINE     4096

/*
 * 线程处理函数：接收客户端表达式、计算、返回结果
 * 参数是指向 int（已连接套接字描述符）的指针
 */
void *handle_client(void *client_socket_ptr) {
    /* 分离线程，线程结束后自动回收资源 */
    pthread_detach(pthread_self());

    int client_fd = *(int *)client_socket_ptr;
    free(client_socket_ptr);   /* 用完即释放 */

    char buff[MAX_LINE];
    memset(buff, 0, sizeof(buff));
    int n = recv(client_fd, buff, sizeof(buff) - 1, 0);
    if (n > 0) {
        buff[n] = '\0';
        printf("[线程 %lu] 收到: %s\n", pthread_self(), buff);

        char res[64];
        calculate(buff, res);
        printf("[线程 %lu] 结果: %s\n", pthread_self(), res);

        send(client_fd, res, strlen(res), 0);
    }

    close(client_fd);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int    socket_fd;
    struct sockaddr_in servaddr;
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

    printf("【实验2 — 多线程】服务器启动，监听端口 %d ...\n", port);

    /* ---- 4. 循环接受连接 + 创建线程 ---- */
    while (1) {
        /*
         * 动态分配内存保存 connected_fd，避免主线程
         * 下一次 accept 覆盖当前值，导致子线程读到错误值
         */
        int *connected_fdp = (int *)malloc(sizeof(int));
        if (connected_fdp == NULL) {
            perror("malloc");
            continue;
        }

        *connected_fdp = accept(socket_fd, NULL, NULL);
        if (*connected_fdp == -1) {
            printf("accept error: %s (errno: %d)\n", strerror(errno), errno);
            free(connected_fdp);
            continue;
        }

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
