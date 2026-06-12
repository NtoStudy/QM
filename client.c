/*
 * client.c  — 客户端程序（实验1、实验2、实验3 通用客户端）
 * 作者：牛文俊
 * 学号：230207224
 * 完成日期：2026-06-12
 * 功能：TCP 客户端，向服务端发送四则运算表达式，接收并展示计算结果
 * 编译命令: gcc -o client client.c
 * 运行命令: ./client <服务器IP> <端口>
 *          示例（连接本机服务）: ./client 127.0.0.1 8888
 * 使用说明：启动后输入四则运算表达式，按下回车发送；输入 q/Q 退出客户端
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 8888   // 默认服务端端口号
#define MAX_LINE     4096   // 收发数据缓冲区最大长度

int main(int argc, char *argv[]) {
    int    socket_fd;                // 客户端套接字文件描述符
    struct sockaddr_in servaddr;     // 服务端地址信息结构体
    char   sendline[MAX_LINE];       // 发送数据缓冲区
    char   recvline[MAX_LINE];       // 接收数据缓冲区

    /* ---- 解析命令行参数，指定服务端IP与端口 ---- */
    const char *server_ip = "127.0.0.1";
    int         port      = DEFAULT_PORT;

    if (argc >= 2) server_ip = argv[1];
    if (argc >= 3) port     = atoi(argv[2]);

    /* ---- 创建 IPv4 协议下的 TCP 流式套接字 ---- */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* ---- 填充服务端地址结构体，转换网络字节序 ---- */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_port        = htons(port);  // 端口转为网络字节序
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* ---- 主动发起 TCP 连接，连接远程服务端 ---- */
    if (connect(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("connect");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    printf("已连接服务器 %s:%d\n", server_ip, port);
    printf("输入表达式（输入 q 退出）:\n");

    /* ---- 循环交互：读取输入、发送表达式、接收计算结果 ---- */
    while (1) {
        printf("> ");
        if (fgets(sendline, MAX_LINE, stdin) == NULL)
            break;

        /* 去除读取到的字符串末尾换行符 */
        sendline[strcspn(sendline, "\n")] = '\0';

        /* 输入 q/Q 退出客户端程序 */
        if (strcmp(sendline, "q") == 0 || strcmp(sendline, "Q") == 0)
            break;

        /* 忽略空输入 */
        if (strlen(sendline) == 0)
            continue;

        /* 向服务端发送运算表达式 */
        if (send(socket_fd, sendline, strlen(sendline), 0) < 0) {
            perror("send");
            break;
        }

        /* 接收服务端返回的计算结果 */
        memset(recvline, 0, sizeof(recvline));
        int n = recv(socket_fd, recvline, sizeof(recvline) - 1, 0);
        if (n <= 0) {
            printf("服务器断开连接\n");
            break;
        }
        recvline[n] = '\0';
        printf("= %s\n", recvline);
    }

    /* 关闭套接字，释放资源 */
    close(socket_fd);
    return 0;
}
