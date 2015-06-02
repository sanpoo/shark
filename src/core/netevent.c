/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

#include "netevent.h"

struct fd_event
{
    int mask;
    event_proc proc;
    void *args;
};

struct event_loop
{
    int max_conn;               //最大能处理的fd个数
    struct fd_event *array;     //每个fd的事件
    int epfd;                   //epoll fd
    struct epoll_event *ee;     //全部的fd事件
};

static struct event_loop g_eventloop;

int add_fd_event(int fd, event_what what, event_proc proc, void *args)
{
    struct fd_event *fe;
    int op;
    struct epoll_event ee;
    int mask;

    if (fd >= g_eventloop.max_conn)
    {
        errno = ERANGE;
        return -1;
    }

    fe = &g_eventloop.array[fd];
    op = (EVNET_NONE == fe->mask) ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0;
    ee.data.fd = fd;
    mask = what | fe->mask; //merge old event

    if (mask & EVENT_READABLE)  ee.events |= EPOLLIN;
    if (mask & EVENT_WRITABLE)  ee.events |= EPOLLOUT;

    if (-1 == epoll_ctl(g_eventloop.epfd, op, fd, &ee))
        return -2;

    fe->mask |= what;
    fe->proc = proc;
    fe->args = args;

    return 0;
}

void del_fd_event(int fd, event_what what)
{
    struct fd_event *fe;
    struct epoll_event ee;
    int mask;

    if (fd >= g_eventloop.max_conn)
        return;

    fe = &g_eventloop.array[fd];
    if (EVNET_NONE == fe->mask)
        return;

    fe->mask &= ~(what);    //what可能是一个组合
    mask = fe->mask;
    ee.events = 0;
    ee.data.fd = fd;

    if (mask & EVENT_READABLE)  ee.events |= EPOLLIN;
    if (mask & EVENT_WRITABLE)  ee.events |= EPOLLOUT;

    if (mask == EVNET_NONE)
        epoll_ctl(g_eventloop.epfd, EPOLL_CTL_DEL, fd, &ee);
    else
        epoll_ctl(g_eventloop.epfd, EPOLL_CTL_MOD, fd, &ee);
}

void event_cycle(int milliseconds)
{
    int i;
    int value;

    value = epoll_wait(g_eventloop.epfd, g_eventloop.ee, g_eventloop.max_conn, milliseconds);
    if (value <= 0)
        return;

    for (i = 0; i < value; i++)
    {
        struct epoll_event *ee = &g_eventloop.ee[i];
        struct fd_event *fe = &g_eventloop.array[ee->data.fd];
        fe->proc(fe->args);
    }
}

/*
    @max_conn:最大能处理的网络连接数
    注意: 如果后端转发或连接跟前端是N:N的关系的话, 这个max_conn需要加倍
*/
void event_loop_init(int max_conn)
{
    g_eventloop.max_conn = max_conn + 128;    //多出来的部分留给系统

    g_eventloop.array = (struct fd_event *)calloc(g_eventloop.max_conn, sizeof(struct fd_event));
    if (NULL == g_eventloop.array)
    {
        printf("Failed to malloc fd g_eventloop\n");
        exit(0);
    }

    g_eventloop.epfd = epoll_create(1024);
    if (-1 == g_eventloop.epfd)
    {
        printf("Failed to create epoll. err:%s\n", strerror(errno));
        exit(0);
    }

    g_eventloop.ee = (struct epoll_event *)malloc(g_eventloop.max_conn * sizeof(struct epoll_event));
    if (NULL == g_eventloop.ee)
    {
        printf("Failed to malloc epoll g_eventloop\n");
        exit(0);
    }
}

