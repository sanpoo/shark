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
#include "conf.h"
#include "net.h"
#include "strop.h"
#include "shm.h"

#include "env.h"

/*
    操作系统相关的变量
*/
int PAGE_SIZE;
int CPU_NUM;

/*
    conf的数据放置到这里
*/
char *g_log_path;           //日志路径
char *g_log_strlevel;       //日志级别
int g_log_reserve;          //日志保留天数

int g_worker_processes;     //系统worker进程个数, 默认为CPU核心数
int g_worker_connections;   //每个worker进程中保持的协程
int g_coro_stacksize;       //协程的栈大小(KB)

char *g_server_ip;          //tcp server绑定ip
int g_server_port;          //tcp 监听端口

/*
    非conf的全局变量放置到这里
*/
int g_master_pid;           //master 进程id

int g_listenfd;             //监听fd
spinlock *g_accept_lock;    //accept fd自旋锁

static void get_worker_env()
{
    char *c = get_conf("worker_processes");

    // 1. worker num
    if (str_case_equal(c, "default"))
        g_worker_processes = CPU_NUM;
    else
        g_worker_processes = atoi(c);

    if (g_worker_processes < 0 || g_worker_processes > 32)
    {
        printf("check shark.conf.g_worker_processes:%d, should default or (0~32)\n", g_worker_processes);
        exit(0);
    }

    // 2.connection
    c = get_conf("worker_connections");
    g_worker_connections = atoi(c);
    if (g_worker_connections <= 0)
    {
        printf("check shark.conf.worker_connections:%d, should > 0\n", g_worker_connections);
        exit(0);
    }

    // 3. coro stacksize
    c = get_conf("coroutine_stacksize");
    g_coro_stacksize = ALIGN(atoi(c) * 1024, PAGE_SIZE) >> 10;
    if (g_coro_stacksize <= 0 || g_coro_stacksize > 1024)
    {
        printf("check shark.conf.coroutine_stacksize:%d, should (0~1024)\n", g_coro_stacksize);
        exit(0);
    }
}

static void get_log_env()
{
    g_log_path = get_conf("log_path");
    g_log_strlevel = get_conf("log_level");
    g_log_reserve = atoi(get_conf("log_reserve_days"));

    if (g_log_reserve <= 0)
        g_log_reserve = 7;
}

static void get_server_env()
{
    char *c = get_conf("server_ip");
    g_server_ip = str_case_equal(c, "default") ? NULL : c;

    c = get_conf("listen");
    g_server_port = atoi(c);
    if (g_server_port <= 0)
    {
        printf("check shark.conf.listen:%d, should uint\n", g_worker_connections);
        exit(0);
    }
}

void print_sys_env_var()
{
    printf("\nPAGE SIZE               : %d\n", PAGE_SIZE);
    printf("CPU NUM                 : %d\n", CPU_NUM);
    printf("log path                : %s\n", g_log_path);
    printf("log level               : %s\n", g_log_strlevel);
    printf("log reserve days        : %d\n", g_log_reserve);
    printf("worker count            : %d\n", g_worker_processes);
    printf("connection per-worker   : %d\n", g_worker_connections);
    printf("coroutine stacksize(KB) : %d\n", g_coro_stacksize);
    printf("server ip               : %s\n", g_server_ip ? g_server_ip : "default");
    printf("server port             : %d\n", g_server_port);
    printf("listen fd               : %d\n", g_listenfd);
    printf("master process pid      : %d\n", g_master_pid);
}

void sys_env_var_init()
{
    PAGE_SIZE = sysconf(_SC_PAGESIZE);
    CPU_NUM = sysconf(_SC_NPROCESSORS_CONF);
    g_master_pid = getpid();
}

/*
    只处理conf中的数据
*/
void conf_env_init()
{
    get_log_env();
    get_worker_env();
    get_server_env();
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

