/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __SYS_HOOK_H__
#define __SYS_HOOK_H__

typedef unsigned int (*sys_sleep)(unsigned int seconds);
typedef int (*sys_usleep)(useconds_t usec);

extern sys_sleep g_sys_sleep;
extern sys_usleep g_sys_usleep;

/*
    以下2个会阻塞进程(线程), 用在非协程环境中, 协程环境使用sleep 或 usleep
 */
#define SLEEP(seconds)  g_sys_sleep(seconds)
#define USLEEP(usec)    g_sys_usleep(usec)

#endif

