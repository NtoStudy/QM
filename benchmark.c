/*
 * benchmark.c  —  并发性能压测工具
 *
 * 编译: gcc -o benchmark benchmark.c -pthread -lm
 * 用法: ./benchmark <服务器IP> <端口> <并发数> <每个客户端请求次数>
 *       例: ./benchmark 127.0.0.1 8888 50 5
 *           表示 50 个并发客户端，每个发 5 次请求
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MAX_LINE 4096
#define EXPR     "4*3/2-2*(9-8)"   // 压测用的固定表达式

/* 每个线程的记录 */
typedef struct {
    int    id;
    int    total_requests;
    double *latencies;       // 每次请求的耗时(秒)
    int    success_count;
    int    fail_count;
} ThreadResult;

static ThreadResult *g_results;
static const char   *g_server_ip;
static int           g_port;

/* 获取当前时间(秒) */
static double now_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/* 单个客户端线程 */
static void *client_thread(void *arg) {
    ThreadResult *r = (ThreadResult *)arg;

    for (int i = 0; i < r->total_requests; i++) {
        /* ---- 创建套接字 + 连接 ---- */
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) { r->fail_count++; continue; }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(g_port);
        if (inet_pton(AF_INET, g_server_ip, &addr.sin_addr) <= 0) {
            close(fd); r->fail_count++; continue;
        }

        double t1 = now_sec();

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(fd); r->fail_count++; continue;
        }

        /* ---- 发送 ---- */
        if (send(fd, EXPR, strlen(EXPR), 0) < 0) {
            close(fd); r->fail_count++; continue;
        }

        /* ---- 接收 ---- */
        char buf[MAX_LINE];
        int n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            close(fd); r->fail_count++; continue;
        }
        buf[n] = '\0';

        double t2 = now_sec();
        close(fd);

        r->latencies[i] = t2 - t1;
        r->success_count++;
    }
    return NULL;
}

/* 排序用的比较函数 */
static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("用法: %s <IP> <端口> <并发数> <每客户端请求次数>\n", argv[0]);
        printf("示例: %s 127.0.0.1 8888 100 5\n", argv[0]);
        return 1;
    }

    g_server_ip = argv[1];
    g_port      = atoi(argv[2]);
    int n_clients = atoi(argv[3]);
    int n_reqs    = atoi(argv[4]);

    printf("=================== 性能压测 ===================\n");
    printf("服务器:       %s:%d\n", g_server_ip, g_port);
    printf("并发客户端数: %d\n", n_clients);
    printf("每客户端请求: %d\n", n_reqs);
    printf("总请求数:     %d\n\n", n_clients * n_reqs);

    /* 初始化结果数组 */
    g_results = calloc(n_clients, sizeof(ThreadResult));
    pthread_t *threads = malloc(n_clients * sizeof(pthread_t));

    for (int i = 0; i < n_clients; i++) {
        g_results[i].id             = i;
        g_results[i].total_requests = n_reqs;
        g_results[i].latencies      = malloc(n_reqs * sizeof(double));
        g_results[i].success_count  = 0;
        g_results[i].fail_count     = 0;
    }

    /* 启动所有并发客户端 */
    double start = now_sec();
    for (int i = 0; i < n_clients; i++) {
        pthread_create(&threads[i], NULL, client_thread, &g_results[i]);
    }

    /* 等待所有线程结束 */
    for (int i = 0; i < n_clients; i++) {
        pthread_join(threads[i], NULL);
    }
    double elapsed = now_sec() - start;

    /* 统计 */
    int total_ok = 0, total_fail = 0;
    double sum_lat = 0, max_lat = 0, min_lat = 999;
    double *all_lat = malloc(n_clients * n_reqs * sizeof(double));
    int idx = 0;

    for (int i = 0; i < n_clients; i++) {
        total_ok  += g_results[i].success_count;
        total_fail += g_results[i].fail_count;
        for (int j = 0; j < g_results[i].success_count; j++) {
            double lat = g_results[i].latencies[j];
            sum_lat += lat;
            if (lat > max_lat) max_lat = lat;
            if (lat < min_lat) min_lat = lat;
            all_lat[idx++] = lat;
        }
    }

    /* 排序算中位数和百分位 */
    qsort(all_lat, idx, sizeof(double), (void *)(
        int (*)(const void *, const void *))(
            (int (*)(const void *, const void *))cmp_double
    ));

    double avg_lat = sum_lat / idx;
    double p50     = all_lat[(int)(idx * 0.5)];
    double p95     = all_lat[(int)(idx * 0.95)];
    double p99     = all_lat[(int)(idx * 0.99)];
    double throughput = total_ok / elapsed;

    printf("==================== 结果 ======================\n");
    printf("总耗时:         %.3f 秒\n", elapsed);
    printf("成功请求:       %d\n", total_ok);
    printf("失败请求:       %d\n", total_fail);
    printf("吞吐量:         %.1f 请求/秒\n", throughput);
    printf("平均延迟:       %.3f 秒\n", avg_lat);
    printf("最小延迟:       %.3f 秒\n", min_lat);
    printf("最大延迟:       %.3f 秒\n", max_lat);
    printf("中位数延迟(P50): %.3f 秒\n", p50);
    printf("P95 延迟:       %.3f 秒\n", p95);
    printf("P99 延迟:       %.3f 秒\n", p99);
    printf("===============================================\n");

    /* 清理 */
    for (int i = 0; i < n_clients; i++) free(g_results[i].latencies);
    free(g_results);
    free(threads);
    free(all_lat);
    return 0;
}
