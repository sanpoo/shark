/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __NETEVENT_H__
#define __NETEVENT_H__

typedef enum
{
    EVNET_NONE = 0x0,
    EVENT_READABLE = 0x1,
    EVENT_WRITABLE = 0x2,
    EVENT_ALL = 0x3
} event_what;

typedef void (*event_proc)(void *args);

int add_fd_event(int fd, event_what what, event_proc proc, void *args);
void del_fd_event(int fd, event_what what);

void event_cycle(int milliseconds);
void event_loop_init(int max_conn);

#endif

