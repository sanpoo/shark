/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "pub.h"
#include "net.h"
#include "str.h"
#include "shm.h"
#include "conf.h"

#include "env.h"

/*
    操作系统相关的变量
*/
int PAGE_SIZE;      //size bytes
int CPU_NUM;

/*
    conf的数据放置到这里
*/
char *g_log_path;           //日志路径
char *g_log_strlevel;       //日志级别
int g_log_reserve_days;     //日志保留天数

int g_worker_processes;     //系统worker进程个数, 默认为CPU核心数
int g_worker_connections;   //每个worker进程中保持的协程
int g_coro_stack_kbytes;    //协程的栈大小(KB)

char *g_server_ip;          //tcp server绑定ip
int g_server_port;          //tcp 监听端口

/*
    非conf的全局变量放置到这里
*/
int g_master_pid;           //master 进程id
int g_listenfd;             //监听fd
spinlock *g_accept_lock;    //accept fd自旋锁

static void set_worker_env()
{
    char *c;

    // 1. worker num
    c = get_raw_conf("worker_processes");
    g_worker_processes = str_equal(c, "default") ? CPU_NUM : atoi(c);
    if (g_worker_processes < 0 || g_worker_processes > MAX_WORKER_PROCESS)
    {
        printf("worker_processes should default or [0~%d], curr:%d\n",
               g_worker_processes, MAX_WORKER_PROCESS);
        exit(0);
    }

    // 2.connection
    c = get_raw_conf("worker_connections");
    g_worker_connections = atoi(c);
    if (g_worker_connections <= 0)
    {
        printf("check worker_connections config. curr:%d, should > 0\n",
               g_worker_connections);
        exit(0);
    }

    // 3. coro stacksize
    c = get_raw_conf("coroutine_stack_sizekbytes");
    g_coro_stack_kbytes = ALIGN(atoi(c) * 1024, PAGE_SIZE) >> 10;
    if (g_coro_stack_kbytes <= 0 || g_coro_stack_kbytes > 10240)
    {
        printf("check coroutine_stack_sizekbyte config. curr:%d, should [%dKB~10MB]\n",
               g_coro_stack_kbytes, PAGE_SIZE >> 10);
        exit(0);
    }
}

static void set_log_env()
{
    g_log_path = get_raw_conf("log_path");
    g_log_strlevel = get_raw_conf("log_level");
    g_log_reserve_days = atoi(get_raw_conf("log_reserve_days"));

    if (g_log_reserve_days <= 0)
        g_log_reserve_days = 7;
}

static void set_server_env()
{
    char *c = get_raw_conf("server_ip");
    g_server_ip = str_equal(c, "default") ? NULL : c;

    c = get_raw_conf("listen");
    g_server_port = atoi(c);
    if (g_server_port <= 0)
    {
        printf("check shark.conf.listen:%d, should uint\n", g_server_port);
        exit(0);
    }
}

void print_env()
{
    printf("PAGE SIZE               : %dKB"LINEFEED, PAGE_SIZE >> 10);
    printf("CPU NUM                 : %d"LINEFEED, CPU_NUM);
    printf("log path                : %s"LINEFEED, g_log_path);
    printf("log level               : %s"LINEFEED, g_log_strlevel);
    printf("log reserve days        : %d"LINEFEED, g_log_reserve_days);
    printf("worker count            : %d"LINEFEED, g_worker_processes);
    printf("connections per-worker  : %d"LINEFEED, g_worker_connections);
    printf("coroutine stack size    : %dKB"LINEFEED, g_coro_stack_kbytes);
    printf("server ip               : %s"LINEFEED, g_server_ip ? g_server_ip : "default");
    printf("server port             : %d"LINEFEED LINEFEED, g_server_port);
}

void sys_env_init()
{
    PAGE_SIZE = sysconf(_SC_PAGESIZE);
    CPU_NUM = sysconf(_SC_NPROCESSORS_CONF);
}

/*
    只处理conf中的数据
*/
void conf_env_init()
{
    set_log_env();
    set_worker_env();
    set_server_env();
}

void tcp_srv_init()
{
    g_listenfd = create_tcp_server(g_server_ip, g_server_port);
    g_accept_lock = shm_alloc(sizeof(spinlock));
    if (NULL == g_accept_lock)
    {
        printf("Failed to alloc global accept lock\n");
        exit(0);
    }
}

