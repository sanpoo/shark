/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __PROCESS_H__
#define __PROCESS_H__

enum PROC_TYPE
{
    MASTER_PROCESS = 0,
    WORKER_PROCESS = 1
};

extern enum PROC_TYPE g_process_type;

extern int g_stop_shark;
extern int g_exit_shark;

/*
    以下三个接口是多有系统都要实现的, 没有需要的初始化直接返回0
    1. master函数可用于worker进程共享的变量
    2. 调用worker时, 该函数在进程而不在协程上下文里, 用g_sys_**代替hook的接口
    3. master与worker成功返回0, 失败返回非0
    4. request_handler函数在任何情况下不要close这个fd, 有错误直接返回即可
*/
struct project
{
    int (*master_init)();
    int (*worker_init)();
    void (*request_handler)(int fd);
};

#define PROJECT_INIT     __attribute__((constructor))
void register_project(int (*master_init)(), int (*worker_init)(),
                     void (*request_handler)(int fd));

void worker_process_cycle();
void master_process_cycle();
void worker_exit_handler(int pid);

void tcp_srv_init();
void process_init();

#endif

