/*
 * server_process.c  — 实验2：多进程并发远程计算器服务器
 *
 * 编译: gcc -o server_process server_process.c calculate.c
 * 运行: ./server_process [端口]     （默认 8888）
 *
 * 说明:
 *   - 父进程 accept 后创建子进程 (fork) 处理客户端
 *   - 子进程处理完客户端后 exit，父进程用 waitpid 回收
 *   - 父子进程需各自关闭不需要的文件描述符，防止资源泄漏
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#include "calculate.h"

#define DEFAULT_PORT 8888
#define MAX_LINE     4096

int main(int argc, char *argv[]) {
    int    socket_fd, connected_fd;
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

    printf("【实验2 — 多进程】服务器启动，监听端口 %d ...\n", port);

    /* ---- 4. 循环接受客户端连接 + 创建子进程处理 ---- */
    while (1) {
        if ((connected_fd = accept(socket_fd, NULL, NULL)) == -1) {
            printf("accept socket error: %s (errno: %d)\n", strerror(errno), errno);
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(connected_fd);
            continue;
        }

        if (pid == 0) {
            /* ========== 子进程 ========== */
            /* 关闭监听套接字 — 子进程不需要监听 */
            close(socket_fd);

            char buff[MAX_LINE];
            memset(buff, 0, sizeof(buff));
            int n = recv(connected_fd, buff, sizeof(buff) - 1, 0);
            if (n > 0) {
                buff[n] = '\0';
                printf("[子进程 %d] 收到: %s\n", getpid(), buff);

                char res[64];
                calculate(buff, res);
                printf("[子进程 %d] 结果: %s\n", getpid(), res);

                send(connected_fd, res, strlen(res), 0);
            }

            close(connected_fd);
            exit(EXIT_SUCCESS);
        } else {
            /* ========== 父进程 ========== */
            /* 关闭已连接套接字 — 父进程不负责通信 */
            close(connected_fd);
            /* 非阻塞回收已终止的子进程，防止僵尸进程 */
            while (waitpid(-1, NULL, WNOHANG) > 0);
        }
    }

    close(socket_fd);
    return 0;
}
