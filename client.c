/*
 * client.c  — 客户端（实验1、2、3 共用）
 *
 * 编译: gcc -o client client.c
 * 运行: ./client <服务器IP> <端口>
 *       例如连接本机服务器: ./client 127.0.0.1 8888
 *
 * 启动后输入表达式（如 "4*3/2-2*(9-8)"），
 * 按回车发送给服务器，等待并显示计算结果。
 * 输入字母 q 退出。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 8888
#define MAX_LINE     4096

int main(int argc, char *argv[]) {
    int    socket_fd;
    struct sockaddr_in servaddr;
    char   sendline[MAX_LINE];
    char   recvline[MAX_LINE];

    /* ---- 解析命令行参数 ---- */
    const char *server_ip = "127.0.0.1";
    int         port      = DEFAULT_PORT;

    if (argc >= 2) server_ip = argv[1];
    if (argc >= 3) port     = atoi(argv[2]);

    /* ---- 创建套接字 ---- */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* ---- 设置服务器地址 ---- */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(port);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* ---- 连接服务器 ---- */
    if (connect(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("connect");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("已连接服务器 %s:%d\n", server_ip, port);
    printf("输入表达式（输入 q 退出）:\n");

    /* ---- 交互循环 ---- */
    while (1) {
        printf("> ");
        if (fgets(sendline, MAX_LINE, stdin) == NULL)
            break;

        /* 去掉末尾换行符 */
        sendline[strcspn(sendline, "\n")] = '\0';

        if (strcmp(sendline, "q") == 0 || strcmp(sendline, "Q") == 0)
            break;

        if (strlen(sendline) == 0)
            continue;

        /* 发送表达式 */
        if (send(socket_fd, sendline, strlen(sendline), 0) < 0) {
            perror("send");
            break;
        }

        /* 接收结果 */
        memset(recvline, 0, sizeof(recvline));
        int n = recv(socket_fd, recvline, sizeof(recvline) - 1, 0);
        if (n <= 0) {
            printf("服务器断开连接\n");
            break;
        }
        recvline[n] = '\0';
        printf("= %s\n", recvline);
    }

    close(socket_fd);
    return 0;
}
