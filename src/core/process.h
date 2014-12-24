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

#define INVALID_PID  -1

struct process
{
    pid_t pid;          // -1表示不存在
    int cpuid;          // 绑定cpuid, 初始化后不变
};

extern struct process g_process[32];
extern int g_process_id;
extern enum PROC_TYPE g_process_type;

extern int g_conn_count;

extern int g_shutdown_shark;
extern int g_exit_shark;
extern int g_all_workers_exit;

void handle_request(int fd);
int project_init();

void master_process_cycle();
void worker_process_cycle();
void process_init();

#endif

