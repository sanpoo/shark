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

static void worker_process_get_status()
{
    pid_t pid;
    int stat;

    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
    {
        WARN("worker process %d exited", pid);
        log_worker_flush_and_reset(pid);
        reset_worker_process(pid);
    }
}

static void master_signal_handler(int signo)
{
    switch (signo)
    {
        case SIGQUIT:
            g_stop_shark = 1;
            break;
        case SIGTERM:
        case SIGINT:
            g_exit_shark = 1;
            break;
        case SIGHUP:
        case SIGUSR1:
        case SIGUSR2:
            break;

        case SIGCHLD:
            worker_process_get_status();
            break;
    }
}

static void worker_signal_handler(int signo)
{
    switch (signo)
    {
        case SIGQUIT:
            g_stop_shark = 1;
            INFO("I am worker, stop accepting and wait for established connections to be handled over");
            break;
        case SIGTERM:
        case SIGINT:
            g_exit_shark = 1;
            INFO("I am worker, ditect exit");
            break;
    }
}

static void signal_handler(int signo)
{
    struct sys_signal *sig;

    for (sig = g_signals; sig->signo != 0; sig++)
    {
        if (sig->signo == signo)
            break;
    }

    switch (g_process_type)
    {
        case MASTER_PROCESS:
            master_signal_handler(signo);
            break;

        case WORKER_PROCESS:
            worker_signal_handler(signo);
            break;

        default:
            break;
    }
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

