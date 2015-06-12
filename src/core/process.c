/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
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

#define INVALID_PID  -1

struct process
{
    pid_t pid;          // -1表示不存在
    int cpuid;          // 绑定cpuid, 初始化后不变
};

static struct process g_process[MAX_WORKER_PROCESS];//子进程信息, 最多支持32个子进程
static int g_process_id;                            //进程id, 包含master进程
enum PROC_TYPE g_process_type;     //进程类型

static int g_conn_count = 1;       //初始化为1, 为0的时候, worker退出
static int g_all_workers_exit = 0; //workers是否都退出
static int g_create_worker = 1;    //当worker异常退出时候

int g_stop_shark = 0;       //worker是否停止接收fd, 0表示否, 其他表示停止
int g_exit_shark = 0;       //是否退出shark系统

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

static int worker_empty()
{
    int i;

    for (i = 0; i < g_worker_processes; i++)
    {
        struct process *p = &g_process[i];
        if (p->pid != INVALID_PID)
            return 0;
    }

    return 1;
}

static pid_t fork_worker(struct process *p)
{
    pid_t pid = fork();

    switch (pid)
    {
        case -1:
            ERR("Failed to fork worker process. master pid:%d", getpid());
            return -1;

        case 0:
            g_process_id = getpid();
            g_process_type = WORKER_PROCESS;
            set_proc_title("shark: worker process");

            if (log_worker_alloc(g_process_id) < 0)
            {
                printf("Failed to alloc log for process:%d\n", g_process_id);
                exit(0);
            }

            if (bind_cpu(p->cpuid))
            {
                ERR("Failed to bind cpu: %d\n", p->cpuid);
                exit(0);
            }

            return 0;

        default:
            p->pid = pid;
            return pid;
    }
}

static void spawn_worker_process()
{
    int i;

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

    request_handler(connfd);
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
        if (unlikely(g_stop_shark))
        {
            WARN("worker stop to accept connect and wait all conntion to be over");
            set_proc_title("shark: worker process is shutting down");
            decrease_conn_and_check();
            break;
        }

        if (unlikely(g_exit_shark))
        {
            WARN("worker direct exit");
            set_proc_title("shark: worker process direct exit");
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
            schedule_timeout(200);
            continue;
        }
        else
            continue;

        increase_conn();
    }
}

void worker_process_cycle()
{
    if (worker_process_init())
        exit(0);

    schedule_init(g_coro_stack_kbytes, g_worker_connections);
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
    if (master_process_init())
    {
        ERR("Failed to init master process, shark exit");
        exit(0);
    }

    INFO("master success running....");

    for (;;)
    {
        if (g_stop_shark == 1)
        {
            WARN("notify worker processes to stop accepting new connections, "
                 "and wait connected connections handled over, then shark exit");
            send_signal_to_workers(SHUTDOWN_SIGNAL);
            g_stop_shark = 2;
        }

        if (g_exit_shark == 1)
        {
            WARN("notify workers processes to direct exit, then shark exit");
            send_signal_to_workers(TERMINATE_SIGNAL);
            g_exit_shark = 2;
        }

        if (g_all_workers_exit == 1)
        {
            WARN("shark will exit...");
            log_scan_write();
            delete_pidfile();
            exit(0);
        }

        if (g_create_worker)
        {
            g_create_worker = 0;
            spawn_worker_process();
            if (g_process_id != g_master_pid)
                break;
        }

        log_scan_write();
        usleep(10000);
    }
}

void reset_worker_process(int pid)
{
    int i;
    for (i = 0; i < g_worker_processes; i++)
    {
        struct process *p = &g_process[i];
        if (p->pid == pid)
            p->pid = INVALID_PID;
    }

    //worker进程退出, 但是并没有收到要shark停止或退出的指令, 表明子进程异常退出
    if (!g_stop_shark && !g_exit_shark)
        g_create_worker = 1;

    if (worker_empty() && (g_stop_shark || g_exit_shark))
        g_all_workers_exit = 1;
}

void process_init()
{
    g_process_id = getpid();
    g_master_pid = g_process_id;
    g_process_type = MASTER_PROCESS;
    set_proc_title("shark: master process");
    create_pidfile(g_master_pid);
    if (log_worker_alloc(g_process_id) < 0)
    {
        printf("Failed to alloc log for process:%d\n", g_process_id);
        exit(0);
    }

    process_struct_init();
}

