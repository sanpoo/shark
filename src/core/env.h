/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __ENV_H__
#define __ENV_H__

#include "spinlock.h"

/*
    宏定义
 */
#define CONF_FILE           "../conf/shark.conf"
#define MASTER_PID_FILE     "../log/shark.pid"
#define SHARK_VER           "shark/1.2.0.20150227"

#define MAX_WORKER_PROCESS  32

/*
    操作系统相关的全局变量
 */
extern char **shark_argv;
extern char **environ;
extern int PAGE_SIZE;
extern int CPU_NUM;

/*
    conf相关的
 */
extern char *g_log_path;            //日志路径
extern char *g_log_strlevel;        //日志级别
extern int g_log_reserve_days;      //日志保留天数

extern int g_worker_processes;      //系统worker进程个数, 默认为CPU核心数
extern int g_worker_connections;    //每个worker进程中保持的协程
extern int g_coro_stack_kbytes;     //协程的栈大小

extern char *g_server_ip;           //tcp server绑定ip
extern int g_server_port;           //tcp 监听端口

/*
    shark相关的全局变量
 */
extern int g_master_pid;            //master 进程id
extern int g_listenfd;              //监听fd
extern spinlock *g_accept_lock;     //accept fd自旋锁

void print_conf();
void print_runtime_var();
void proc_title_init(char **argv);
void sys_env_init();
void conf_env_init();
void tcp_srv_init();

#endif

