/*
 * server_threadpool.c  — 实验3：基于线程池的远程计算器服务器
 *
 * 编译: gcc -o server_threadpool server_threadpool.c calculate.c -pthread
 * 运行: ./server_threadpool [端口] [线程数]   （默认 8888, 线程数 5）
 *
 * 说明:
 *   - 服务启动时创建固定数量的线程组成线程池
 *   - 主线程接收客户端连接，将任务放入共享队列
 *   - 线程池中的线程从队列取出任务处理，实现线程复用
 *   - 使用互斥锁保护任务队列，使用条件变量避免忙等
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "calculate.h"

#define DEFAULT_PORT  8888
#define DEFAULT_THREADS 5
#define QUEUE_CAPACITY  100
#define MAX_LINE        4096

/* ======================== 数据结构 ======================== */

/* 计算任务 */
typedef struct {
    int connected_fd;        /* 已连接套接字描述符 */
} Task;

/* 环形任务队列 */
typedef struct {
    Task  *tasks;            /* 任务数组 */
    int    capacity;         /* 容量 */
    int    size;             /* 当前任务数 */
    int    front;            /* 队头索引（出队位置） */
    int    rear;             /* 队尾索引（入队位置） */
} TaskQueue;

/* 线程池 */
typedef struct {
    pthread_t  *threads;     /* 线程数组 */
    int         thread_count;/* 线程数量 */
    TaskQueue   task_queue;  /* 任务队列 */
    int         shutdown;    /* 1 表示关闭线程池 */
} ThreadPool;

/* ======================== 全局互斥锁 & 条件变量 ======================== */

static pthread_mutex_t  task_mutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   task_cond   = PTHREAD_COND_INITIALIZER;

/* ======================== 任务队列操作 ======================== */

/* 初始化任务队列 */
static void initTaskQueue(TaskQueue *q, int capacity) {
    q->tasks    = (Task *)malloc(sizeof(Task) * capacity);
    q->capacity = capacity;
    q->size     = 0;
    q->front    = 0;
    q->rear     = 0;
}

/* 添加任务（线程安全） */
static void enqueueTask(TaskQueue *q, Task task) {
    pthread_mutex_lock(&task_mutex);

    if (q->size < q->capacity) {
        q->tasks[q->rear] = task;
        q->rear = (q->rear + 1) % q->capacity;
        q->size++;
    } else {
        printf("任务队列已满，丢弃连接 %d\n", task.connected_fd);
        close(task.connected_fd);
    }

    /* 唤醒一个等待中的工作线程 */
    pthread_cond_signal(&task_cond);
    pthread_mutex_unlock(&task_mutex);
}

/* 取出任务（线程安全）— 如果队列为空则阻塞等待 */
static Task dequeueTask(TaskQueue *q) {
    pthread_mutex_lock(&task_mutex);

    /* 当队列为空且线程池尚未关闭时，等待条件变量 */
    while (q->size == 0 && q->capacity > 0) {
        pthread_cond_wait(&task_cond, &task_mutex);
    }

    Task task;
    if (q->size > 0) {
        task = q->tasks[q->front];
        q->front = (q->front + 1) % q->capacity;
        q->size--;
    } else {
        task.connected_fd = -1;   /* 无效任务 */
    }

    pthread_mutex_unlock(&task_mutex);
    return task;
}

/* ======================== 工作线程函数 ======================== */

static void *threadFunc(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;

    while (1) {
        Task task = dequeueTask(&(pool->task_queue));

        if (task.connected_fd == -1) {
            /* 若 shutdown 已置位，dequeueTask 可能返回 -1，线程退出 */
            break;
        }

        char buff[MAX_LINE];
        memset(buff, 0, sizeof(buff));
        int n = recv(task.connected_fd, buff, sizeof(buff) - 1, 0);
        if (n > 0) {
            buff[n] = '\0';
            printf("[线程池 %lu] 收到: %s\n", pthread_self(), buff);

            char res[64];
            calculate(buff, res);
            printf("[线程池 %lu] 结果: %s\n", pthread_self(), res);

            send(task.connected_fd, res, strlen(res), 0);
        }

        close(task.connected_fd);
    }

    pthread_exit(NULL);
}

/* ======================== 主函数 ======================== */

int main(int argc, char *argv[]) {
    int    socket_fd;
    struct sockaddr_in servaddr;
    int    port          = DEFAULT_PORT;
    int    thread_count  = DEFAULT_THREADS;

    if (argc >= 2) port         = atoi(argv[1]);
    if (argc >= 3) thread_count = atoi(argv[2]);

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

    /* ---- 4. 初始化线程池 ---- */
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

    /* ---- 5. 主线程接收连接，入队任务 ---- */
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

    /* ---- 6. 清理（实际使用时不会执行到这里，仅演示） ---- */
    pool.shutdown = 1;
    pthread_cond_broadcast(&task_cond);
    for (int i = 0; i < thread_count; i++) {
        pthread_join(pool.threads[i], NULL);
    }

    free(pool.threads);
    free(pool.task_queue.tasks);
    close(socket_fd);
    return 0;
}
