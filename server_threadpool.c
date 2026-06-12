/*
 * server_threadpool.c  — 实验3：基于线程池的远程计算器服务器
 * 作者：牛文俊
 * 学号：230207224
 * 完成日期：2026-06-12
 * 功能：采用线程池模型实现并发服务端，复用线程处理客户端请求，提升并发性能
 * 编译命令: gcc -o server_threadpool server_threadpool.c calculate.c -pthread
 * 运行命令: ./server_threadpool [端口] [线程数]  默认端口8888，默认线程数5
 * 程序说明:
 *   1. 程序启动时预先创建固定数量工作线程，构建线程池
 *   2. 主线程作为生产者接收客户端连接，将任务加入环形任务队列
 *   3. 池内线程作为消费者从队列取任务执行，实现线程复用
 *   4. 使用互斥锁保证队列线程安全，配合条件变量避免线程忙等待
 *   5. 编译需添加 -pthread 链接线程库
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "calculate.h"   // 引入四则运算计算函数头文件

#define DEFAULT_PORT    8888    // 默认监听端口
#define DEFAULT_THREADS 5       // 线程池默认工作线程数量
#define QUEUE_CAPACITY  100     // 任务队列最大容量
#define MAX_LINE        4096    // 数据收发缓冲区最大长度

/* ======================== 自定义数据结构 ======================== */
/**
 * @brief 任务结构体，封装单个客户端连接任务
 */
typedef struct {
    int connected_fd;        // 客户端已连接套接字描述符
} Task;

/**
 * @brief 环形任务队列结构体，用于存放待处理任务
 */
typedef struct {
    Task  *tasks;            // 任务数组
    int    capacity;         // 队列最大容量
    int    size;             // 当前队列中任务数量
    int    front;            // 队头索引，任务出队位置
    int    rear;             // 队尾索引，任务入队位置
} TaskQueue;

/**
 * @brief 线程池结构体，统一管理线程数组、任务队列与运行状态
 */
typedef struct {
    pthread_t  *threads;     // 工作线程数组
    int         thread_count;// 线程池内线程总数
    TaskQueue   task_queue;  // 关联的任务队列
    int         shutdown;    // 线程池关闭标记，1表示关闭
} ThreadPool;

/* ======================== 全局同步变量 ======================== */
static pthread_mutex_t  task_mutex  = PTHREAD_MUTEX_INITIALIZER;  // 任务队列互斥锁
static pthread_cond_t   task_cond   = PTHREAD_COND_INITIALIZER;   // 条件变量，唤醒阻塞线程

/* ======================== 任务队列操作函数 ======================== */
/**
 * @brief 初始化环形任务队列
 * @param q 任务队列指针
 * @param capacity 队列容量
 */
static void initTaskQueue(TaskQueue *q, int capacity) {
    q->tasks    = (Task *)malloc(sizeof(Task) * capacity);
    q->capacity = capacity;
    q->size     = 0;
    q->front    = 0;
    q->rear     = 0;
}

/**
 * @brief 向任务队列添加任务（线程安全）
 * @param q 任务队列指针
 * @param task 待加入的任务
 */
static void enqueueTask(TaskQueue *q, Task task) {
    pthread_mutex_lock(&task_mutex);  // 加锁保护共享队列

    if (q->size < q->capacity) {
        q->tasks[q->rear] = task;
        q->rear = (q->rear + 1) % q->capacity;  // 取模实现环形队列
        q->size++;
    } else {
        // 队列已满，丢弃当前连接
        printf("任务队列已满，丢弃连接 %d\n", task.connected_fd);
        close(task.connected_fd);
    }

    pthread_cond_signal(&task_cond);  // 唤醒一个等待的工作线程
    pthread_mutex_unlock(&task_mutex); // 解锁
}

/**
 * @brief 从任务队列取出任务（线程安全），队列为空则阻塞等待
 * @param q 任务队列指针
 * @return 取出的任务
 */
static Task dequeueTask(TaskQueue *q) {
    pthread_mutex_lock(&task_mutex);

    // 队列为空且线程池未关闭，线程阻塞等待条件信号
    while (q->size == 0 && q->capacity > 0) {
        pthread_cond_wait(&task_cond, &task_mutex);
    }

    Task task;
    if (q->size > 0) {
        task = q->tasks[q->front];
        q->front = (q->front + 1) % q->capacity;
        q->size--;
    } else {
        task.connected_fd = -1;  // 标记无效任务
    }

    pthread_mutex_unlock(&task_mutex);
    return task;
}

/* ======================== 工作线程入口函数 ======================== */
/**
 * @brief 线程池工作线程执行函数，循环从队列获取并处理任务
 * @param arg 线程池结构体指针
 * @return 无返回值
 */
static void *threadFunc(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;

    while (1) {
        Task task = dequeueTask(&(pool->task_queue));
        if (task.connected_fd == -1) {
            break;  // 收到无效任务，线程退出
        }

        char buff[MAX_LINE];
        memset(buff, 0, sizeof(buff));
        // 接收客户端发送的运算表达式
        int n = recv(task.connected_fd, buff, sizeof(buff) - 1, 0);
        if (n > 0) {
            buff[n] = '\0';
            printf("[线程池 %lu] 收到: %s\n", pthread_self(), buff);

            // 调用计算函数完成运算
            char res[64];
            calculate(buff, res);
            printf("[线程池 %lu] 结果: %s\n", pthread_self(), res);

            // 向客户端返回计算结果
            send(task.connected_fd, res, strlen(res), 0);
        }

        close(task.connected_fd);  // 关闭套接字，释放资源
    }

    pthread_exit(NULL);
}

/* ======================== 主函数：服务端入口 ======================== */
int main(int argc, char *argv[]) {
    int    socket_fd;               // 服务端监听套接字
    struct sockaddr_in servaddr;    // 服务端地址结构体
    int    port          = DEFAULT_PORT;
    int    thread_count  = DEFAULT_THREADS;

    // 解析命令行参数，自定义端口与线程池线程数
    if (argc >= 2) port         = atoi(argv[1]);
    if (argc >= 3) thread_count = atoi(argv[2]);

    /* ---- 1. 创建IPv4协议下的TCP流式套接字 ---- */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create socket error: %s (errno: %d)\n", strerror(errno), errno);
        exit(EXIT_FAILURE);
    }

    /* ---- 2. 绑定本机IP与监听端口 ---- */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;                // IPv4协议簇
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);      // 绑定本机所有网卡
    servaddr.sin_port        = htons(port);            // 端口转为网络字节序

    if (bind(socket_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s (errno: %d)\n", strerror(errno), errno);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* ---- 3. 开启端口监听，设置连接队列长度 ---- */
    if (listen(socket_fd, 10) == -1) {
        printf("listen socket error: %s (errno: %d)\n", strerror(errno), errno);
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    /* ---- 4. 初始化线程池与任务队列，批量创建工作线程 ---- */
    ThreadPool pool;
    pool.thread_count = thread_count;
    pool.threads      = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool.shutdown     = 0;
    initTaskQueue(&(pool.task_queue), QUEUE_CAPACITY);

    for (int i = 0; i < thread_count; i++) {
        pthread_create(&(pool.threads[i]), NULL, threadFunc, &pool);
    }

    printf("【实验3 — 线程池】服务器启动，监听端口 %d，线程池大小 %d\n",
           port, thread_count);

    /* ---- 5. 主线程循环接收客户端连接，将任务加入队列 ---- */
    while (1) {
        int connected_fd = accept(socket_fd, NULL, NULL);
        if (connected_fd == -1) {
            printf("accept error: %s (errno: %d)\n", strerror(errno), errno);
            continue;
        }

        Task task;
        task.connected_fd = connected_fd;
        enqueueTask(&(pool.task_queue), task);
    }

    /* ---- 6. 资源清理（正常运行不会执行，仅作代码演示） ---- */
    pool.shutdown = 1;
    pthread_cond_broadcast(&task_cond);  // 唤醒所有阻塞线程
    for (int i = 0; i < thread_count; i++) {
        pthread_join(pool.threads[i], NULL);
    }

    free(pool.threads);
    free(pool.task_queue.tasks);
    close(socket_fd);
    return 0;
}
