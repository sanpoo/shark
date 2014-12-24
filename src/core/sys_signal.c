/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "log.h"
#include "env.h"
#include "process.h"
#include "sys_signal.h"

struct sys_signal
{
    int signo;
    char *signame;
    char *name;
    void (*handler)(int signo);
};

static void signal_handler(int signo);

/*
    master进程
    QUIT        不再接受新连接, 从容关闭工作进程, master等待全部子进程关闭完成
    TERM, INT   快速关闭
    HUP         重载配置, 用新的配置开始新的工作进程, 从容关闭旧的工作进程
    USR1        重新打开日志文件
    USR2        平滑升级可执行程序

    worker进程
    QUIT	    从容关闭
    TERM, INT	快速关闭
*/
struct sys_signal g_signals[] =
{
    {SIGQUIT,   "SIGQUIT",      	"stop&quit",    signal_handler},
    {SIGTERM,   "SIGTERM",          "direct-quit",  signal_handler},
    {SIGINT,    "SIGINT",           "direct-quit",  signal_handler},
    {SIGHUP,    "SIGHUP",       	"reload",   	signal_handler},
    {SIGUSR1,   "SIGUSR1",      	"reopen",   	signal_handler},
    {SIGUSR2,   "SIGUSR2",      	"change-bin",	signal_handler},

    {SIGCHLD,   "SIGCHLD",          "",         	signal_handler},
    {SIGSYS,    "SIGSYS,ignore",    "", 			SIG_IGN},
    {SIGPIPE,   "SIGPIPE,ignore", 	"", 			SIG_IGN},
    {0, 		NULL,   			"", 			NULL}
};

static void master_signal_handle(int signo, char **action)
{
    switch (signo)
    {
        case SIGQUIT:
            g_shutdown_shark = 1;
            *action = ", stop accept and wait all connection hander over";
            break;
        case SIGTERM:
        case SIGINT:
            g_exit_shark = 1;
            *action = ", stop accept and DO NOT wait all connection hander over, then exit";
            break;
        case SIGHUP:
            break;
        case SIGUSR1:
            break;
        case SIGUSR2:
            break;
    }
}

static void worker_signal_handle(int signo, char **action)
{
    switch (signo)
    {
        case SIGQUIT:
            g_shutdown_shark = 1;
            *action = ", stop accept and wait all conn hander over";
            break;
        case SIGTERM:
        case SIGINT:
            g_exit_shark = 1;
            *action = ", stop accept and DO NOT wait all conn hander over, then exit";
            break;
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

static void worker_process_reset(int pid)
{
    int i;
    for (i = 0; i < g_worker_processes; i++)
    {
        struct process *p = &g_process[i];
        if (p->pid == pid)
            p->pid = INVALID_PID;
    }

    if (worker_empty())
        g_all_workers_exit = 1;
}

static void process_get_status()
{
    pid_t pid;
    int stat;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {
        WARN("worker process %d exited", pid);
        log_worker_flush_and_reset(pid);
        worker_process_reset(pid);
    }
}

static void signal_handler(int signo)
{
    struct sys_signal *sig;
    char *action = "";

    for (sig = g_signals; sig->signo != 0; sig++)
    {
        if (sig->signo == signo)
            break;
    }

    switch (g_process_type)
    {
        case MASTER_PROCESS:
            master_signal_handle(signo, &action);
            break;

        case WORKER_PROCESS:
            worker_signal_handle(signo, &action);
            break;

        default:
            break;
    }

    if (signo == SIGCHLD)
    {
        process_get_status();
        g_shutdown_shark = 1;   //有一个进程退出则所有都退出, 后面再优化
    }

    INFO("signal %d (%s) received%s", signo, sig->signame, action);
}

void sys_signal_init()
{
    struct sys_signal *sig;
    struct sigaction sa;

    for (sig = g_signals; sig->signo != 0; sig++)
    {
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(sig->signo, &sa, NULL) == -1)
        {
            printf("sigaction(%s) failed\n", sig->signame);
            exit(0);
        }
    }
}

