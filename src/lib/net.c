/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

int set_nonblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;

    if (flags & O_NONBLOCK)
        return 0;

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    return 0;
}

static inline int set_tcp_no_delay(int fd, int val)
{
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) == -1)
        return -1;

    return 0;
}

int enable_tcp_no_delay(int fd)
{
    return set_tcp_no_delay(fd, 1);
}

int disable_tcp_no_delay(int fd)
{
    return set_tcp_no_delay(fd, 0);
}

int set_keep_alive(int fd, int interval)
{
    int val = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1)
        return -1;

    val = interval;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &val, sizeof(val)) < 0)
        return -1;

    val = interval / 3;
    if (val == 0)
        val = 1;

    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &val, sizeof(val)) < 0)
        return -1;

    val = 3;
    if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &val, sizeof(val)) < 0)
        return -1;

    return 0;
}

int set_reuse_addr(int fd)
{
    int yes = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        return -1;

    return 0;
}

unsigned fd_to_nl(int fd)
{
    struct sockaddr_in sa;
    socklen_t addrlen = sizeof(sa);

    if (getpeername(fd, (struct sockaddr *)&sa, &addrlen))
        return htonl(INADDR_ANY);

    return sa.sin_addr.s_addr;
}

char *get_peer_ip(int fd)
{
    struct in_addr addr;

    addr.s_addr = fd_to_nl(fd);
    return inet_ntoa(addr);
}

unsigned ip_to_nl(const char *ip)
{
    struct in_addr s;

    if (!ip)
        return htonl(INADDR_ANY);

    if (1 != inet_pton(AF_INET, ip, &s))
        return htonl(INADDR_ANY);

    return s.s_addr;
}

