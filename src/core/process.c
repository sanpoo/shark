/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>

#include "pub.h"
#include "env.h"
#include "log.h"
#include "util.h"
#include "coro_sched.h"
#include "netevent.h"
#include "sys_hook.h"
#include "sys_signal.h"

#include "process.h"

struct process g_process[32];   //子进程信息, 最多支持32个子进程
int g_process_id;               //进程id, 包含master进程
enum PROC_TYPE g_process_type;  //进程类型

int g_conn_count = 1;           //初始化为1, 为0的时候, worker退出

int g_shutdown_shark = 0;       //worker是否停止接收fd, 0表示否, 其他表示停止
int g_exit_shark = 0;           //是否退出shark系统
int g_all_workers_exit = 0;     //workers是否都退出

static void process_struct_init()
{
    int i;
    for (i = 0; i < g_worker_processes; i++)
    {
        struct process *p = &g_process[i];
        p->pid = INVALID_PID;
        p->cpuid = i % CPU_NUM;
    }
}

static pid_t fork_worker(struct process *p)
{
    pid_t pid = fork();

    switch (pid)
    {
        case -1:
            printf("Failed to fork. my pid:%d\n", getpid());
            return -1;

        case 0:
            g_process_id = getpid();
            g_process_type = WORKER_PROCESS;

            if (bind_cpu(p->cpuid))
                printf("Failed to bind cpu: %d\n", p->cpuid);
            //else
            //printf("succ to bind cpu: %d\n", p->cpuid);

            return 0;

        default:
            p->pid = pid;
            //printf("succ to fork. my pid:%d, my child pid %d\n", getpid(), pid);
            return pid;
    }
}

static void spawn_worker_process()
{
    int i;

    g_process_id = getpid();
    g_process_type = MASTER_PROCESS;

    for (i = 0; i < g_worker_processes; i++)
    {
        struct process *p = &g_process[i];
        if (p->pid != INVALID_PID)
            continue;

        if (0 == fork_worker(p))
            break;
    }
}

static inline void increase_conn()
{
    g_conn_count++;
}

static inline void decrease_conn_and_check()
{
    if (--g_conn_count == 0)
    {
        WARN("worker exit now");
        exit(0);
    }
}

static void handle_connection(void *args)
{
    int connfd = (int)(intptr_t)args;

    handle_request(connfd);
    close(connfd);
    decrease_conn_and_check();
}

static inline int worker_can_accept()
{
    return g_conn_count < g_worker_connections;
}

static int worker_accept()
{
    struct sockaddr addr;
    socklen_t addrlen;
    int connfd;

    if (likely(g_worker_processes > 1))
    {
        if (worker_can_accept() && spin_trylock(g_accept_lock))
        {
            connfd = accept(g_listenfd, &addr, &addrlen);
            spin_unlock(g_accept_lock);
            return connfd;
        }

        return 0;
    }
    else
        connfd = accept(g_listenfd, &addr, &addrlen);

    return connfd;
}

static void worker_accept_proc(void *args)
{
    int connfd;

    for (;;)
    {
        if (unlikely(g_shutdown_shark))
        {
            WARN("worker stop to accept connect and wait all conntion to be over");
            decrease_conn_and_check();
            break;
        }

        if (unlikely(g_exit_shark))
        {
            WARN("worker direct exit");
            exit(0);
        }

        connfd = worker_accept();
        if (likely(connfd > 0))
        {
            if (dispatch_coro(handle_connection, (void *)(intptr_t)connfd))
            {
                WARN("system busy to handle request.");
                close(connfd);
                continue;
            }
        }
        else if (connfd == 0)
        {
            schedule_timeout(500);
            continue;
        }
        else
            continue;

        increase_conn();
    }
}

void worker_process_cycle()
{
    if (0 != project_init())
        exit(0);

    schedule_init(g_coro_stacksize, g_worker_connections);
    event_loop_init(g_worker_connections);
    dispatch_coro(worker_accept_proc, NULL);
    INFO("worker success running....");
    schedule_cycle();
}

static void send_signal_to_workers(int signo)
{
    int i;

    for (i = 0; i < g_worker_processes; i++)
    {
        struct process *p = &g_process[i];
        if (p->pid != INVALID_PID)
        {
            if (kill(p->pid, signo) == -1)
                ERR("Failed to send signal %d to child pid:%d", signo, p->pid);
            else
                INFO("succ to send signal %d to child pid:%d", signo, p->pid);
        }
    }
}

void master_process_cycle()
{
    INFO("master success running....");

    for (;;)
    {
        if (g_shutdown_shark)
        {
            WARN("notify workers to stop accept new connection and wait accepted connection to be handled, then exit");
            send_signal_to_workers(SHUTDOWN_SIGNAL);
            g_shutdown_shark = 0;
        }

        if (g_exit_shark)
        {
            WARN("notify workers to direct exit");
            send_signal_to_workers(TERMINATE_SIGNAL);
            g_exit_shark = 0;
        }

        if (g_all_workers_exit)
        {
            WARN("shark will exit...");
            log_scan_write();
            delete_pidfile();
            exit(0);
        }

        log_scan_write();
        USLEEP(10000);
    }
}

void process_init()
{
    process_struct_init();
    spawn_worker_process();
}

