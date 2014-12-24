/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __UTIL_H__
#define __UTIL_H__

#include <sys/time.h>

int log2n(size_t n);
int log_page_order(size_t n);
struct tm *get_tm();
int bind_cpu(int cpuid);
void sys_daemon();
void sys_res_init();
int create_pidfile(int pid);
int read_pidfile();
void delete_pidfile();

#endif

