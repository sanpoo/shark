/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __CORO_SCHED_H__
#define __CORO_SCHED_H__

typedef void (*coro_func)(void *args);

void schedule_cycle();
int dispatch_coro(coro_func func, void *args);
void yield();
void schedule_timeout(int seconds);
int is_wakeup_by_timeout();

void wakeup_coro(void *args);
void wakeup_coro_priority(void *args);
void *current_coro();

void schedule_init(size_t stack_kbytes, size_t max_coro_size);

#endif

