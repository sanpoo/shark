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

void handle_request(int fd);
int project_init();

void worker_process_cycle();
void master_process_cycle();
void reset_worker_process(int pid);
void process_init();

#endif

