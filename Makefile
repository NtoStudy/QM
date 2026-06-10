#
# Makefile  — 一键编译所有实验程序
#
# 使用方法:
#   make         编译全部
#   make server  只编译实验1（单线程服务器 + 客户端）
#   make clean   删除编译产物
#

CC = gcc
CFLAGS = -Wall -g
LIBS = -pthread

# 所有目标
TARGETS = client server server_process server_thread server_threadpool

all: $(TARGETS)

# 客户端（无外部依赖）
client: client.c
	$(CC) $(CFLAGS) -o client client.c

# 三个服务器都需要链接 calculate.o
# 实验1 — 单线程
server: server.c calculate.c calculate.h
	$(CC) $(CFLAGS) -o server server.c calculate.c

# 实验2 — 多进程
server_process: server_process.c calculate.c calculate.h
	$(CC) $(CFLAGS) -o server_process server_process.c calculate.c

# 实验2 — 多线程
server_thread: server_thread.c calculate.c calculate.h
	$(CC) $(CFLAGS) $(LIBS) -o server_thread server_thread.c calculate.c

# 实验3 — 线程池
server_threadpool: server_threadpool.c calculate.c calculate.h
	$(CC) $(CFLAGS) $(LIBS) -o server_threadpool server_threadpool.c calculate.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean
