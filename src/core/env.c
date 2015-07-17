/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "shark.h"
#include "str.h"
#include "conf.h"

#include "env.h"

/*
    操作系统相关的变量
*/
int PAGE_SIZE;
int CPU_NUM;

/*
    conf的数据放置到这里
*/
char *g_log_path;               //日志路径
char *g_log_strlevel;           //日志级别
int g_log_reserve_days;         //日志保留天数

int g_worker_processes;         //系统worker进程个数, 默认为CPU核心数
int g_worker_connections;       //每个worker进程中保持的协程
int g_coro_stack_kbytes;        //协程的栈大小(KB)

char *g_server_ip;              //tcp server绑定ip
int g_server_port;              //tcp 监听端口

static void set_log_env()
{
    char *c;

    g_log_path = get_raw_conf("log_path");
    g_log_strlevel = get_raw_conf("log_level");

    c = get_raw_conf("log_reserve_days");
    g_log_reserve_days = str_equal(c, "default") ? 36500 : atoi(c);
    if (g_log_reserve_days <= 0)
    {
        printf("check log_reserve_days config:%s, should default or > 0\n", c);
        exit(0);
    }
}

static void set_worker_env()
{
    char *c;

    // 1. worker_processes
    c = get_raw_conf("worker_processes");
    g_worker_processes = str_equal(c, "default") ? CPU_NUM : atoi(c);
    if (g_worker_processes < 0 || g_worker_processes > MAX_WORKER_PROCESS)
    {
        printf("check worker_processes config:%d, should default or [0~%d]\n",
               g_worker_processes, MAX_WORKER_PROCESS);
        exit(0);
    }

    // 2.worker_connections
    c = get_raw_conf("worker_connections");
    g_worker_connections = atoi(c);
    if (g_worker_connections <= 0)
    {
        printf("check worker_connections config:%d, should > 0\n",
               g_worker_connections);
        exit(0);
    }

    // 3. coroutine_stack_kbytes
    c = get_raw_conf("coroutine_stack_kbytes");
    g_coro_stack_kbytes = str_equal(c, "default") ? PAGE_SIZE >> 10 :
                          ALIGN(atoi(c) * 1024, PAGE_SIZE) >> 10;
    if (g_coro_stack_kbytes <= 0 || g_coro_stack_kbytes > 10240)
    {
        printf("check coroutine_stack_kbytes config:%d, should [%dKB~10MB]\n",
               g_coro_stack_kbytes, PAGE_SIZE >> 10);
        exit(0);
    }
}

static void set_server_env()
{
    char *head, *loop;

    g_server_ip = NULL;

    head = loop = get_raw_conf("listen");
    while (*loop)
    {
        if (*loop == ':')
        {
            g_server_ip = head;
            *loop = 0;
            head = loop + 1;
        }

        loop++;
    }

    g_server_port = atoi(head);
    if (g_server_port <= 0)
    {
        printf("check listen config:%d, should > 0\n", g_server_port);
        exit(0);
    }
}

void print_env()
{
    printf("PAGE_SIZE              : %dKB\n", PAGE_SIZE >> 10);
    printf("CPU_NUM                : %d\n", CPU_NUM);
    printf("log_path               : %s\n", g_log_path);
    printf("log_level              : %s\n", g_log_strlevel);
    printf("log_reserve_days       : %d\n", g_log_reserve_days);
    printf("worker_processes       : %d\n", g_worker_processes);
    printf("worker_connections     : %d\n", g_worker_connections);
    printf("coroutine_stack_kbytes : %dKB\n", g_coro_stack_kbytes);
    printf("listen                 : %s:%d\n",
           g_server_ip ? g_server_ip : "localhost", g_server_port);
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

