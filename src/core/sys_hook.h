/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __SYS_HOOK_H__
#define __SYS_HOOK_H__

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef int (*sys_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
typedef int (*sys_accept)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

typedef ssize_t (*sys_read)(int fd, void *buf, size_t count);
typedef ssize_t (*sys_readv)(int fd, const struct iovec *iov, int iovcnt);
typedef ssize_t (*sys_recv)(int sockfd, void *buf, size_t len, int flags);
typedef ssize_t (*sys_recvfrom)(int sockfd, void *buf, size_t len, int flags,
                                struct sockaddr *src_addr, socklen_t *addrlen);
typedef ssize_t (*sys_recvmsg)(int sockfd, struct msghdr *msg, int flags);

typedef ssize_t (*sys_write)(int fd, const void *buf, size_t count);
typedef ssize_t (*sys_writev)(int fd, const struct iovec *iov, int iovcnt);
typedef ssize_t (*sys_send)(int sockfd, const void *buf, size_t len, int flags);
typedef ssize_t (*sys_sendto)(int sockfd, const void *buf, size_t len, int flags,
                              const struct sockaddr *dest_addr, socklen_t addrlen);
typedef ssize_t (*sys_sendmsg)(int sockfd, const struct msghdr *msg, int flags);

typedef ssize_t (*sys_sendfile)(int out_fd, int in_fd, off_t *offset, size_t count);

extern sys_connect g_sys_connect;
extern sys_accept g_sys_accept;

extern sys_read g_sys_read;
extern sys_readv g_sys_readv;
extern sys_recv g_sys_recv;
extern sys_recvfrom g_sys_recvfrom;
extern sys_recvmsg g_sys_recvmsg;

extern sys_write g_sys_write;
extern sys_writev g_sys_writev;
extern sys_send g_sys_send;
extern sys_sendto g_sys_sendto;
extern sys_sendmsg g_sys_sendmsg;

extern sys_sendfile g_sys_sendfile;

#endif

