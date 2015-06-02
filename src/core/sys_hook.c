/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "net.h"
#include "netevent.h"
#include "coro_sched.h"

#include "sys_hook.h"

#define HOOK_SYSCALL(name) g_sys_##name = (sys_##name)dlsym(RTLD_NEXT, #name)

#define CONN_TIMEOUT      (10 * 1000)
#define ACCEPT_TIMEOUT    (3 * 1000)

#define READ_TIMEOUT      (10 * 1000)
#define READV_TIMEOUT     (10 * 1000)
#define RECV_TIMEOUT      (10 * 1000)
#define RECVFROM_TIMEOUT  (10 * 1000)
#define RECVMSG_TIMEOUT   (10 * 1000)

#define WRITE_TIMEOUT     (10 * 1000)
#define WRITEV_TIMEOUT    (10 * 1000)
#define SEND_TIMEOUT      (10 * 1000)
#define SENDTO_TIMEOUT    (10 * 1000)
#define SENDMSG_TIMEOUT   (10 * 1000)

#define KEEP_ALIVE        60


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

sys_sleep g_sys_sleep = NULL;
sys_usleep g_sys_usleep = NULL;

static sys_connect g_sys_connect = NULL;
static sys_accept g_sys_accept = NULL;

static sys_read g_sys_read = NULL;
static sys_readv g_sys_readv = NULL;
static sys_recv g_sys_recv = NULL;
static sys_recvfrom g_sys_recvfrom = NULL;
static sys_recvmsg g_sys_recvmsg = NULL;

static sys_write g_sys_write = NULL;
static sys_writev g_sys_writev = NULL;
static sys_send g_sys_send = NULL;
static sys_sendto g_sys_sendto = NULL;
static sys_sendmsg g_sys_sendmsg = NULL;

#define fd_not_ready() ((EAGAIN == errno) || (EWOULDBLOCK == errno))

static void event_rw_callback(void *args)
{
    wakeup_coro(args);
}

static void event_conn_callback(void *args)
{
    wakeup_coro_priority(args);
}

unsigned int sleep(unsigned int seconds)
{
    schedule_timeout(seconds * 1000);
    return 0;
}

int usleep(useconds_t useconds)
{
    yield();
    return 0;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int ret;
    int flags;
    socklen_t len;

    set_nonblock(sockfd);

    /*
        connect < 0 && errno == EINPROGRESS才需要跟踪fd是否可写
        否则其他情形都是错误的, 直接返回
    */
    ret = g_sys_connect(sockfd, addr, addrlen);
    if (0 == ret)   //succ
        return 0;

    if (ret < 0 && errno != EINPROGRESS)
        return -1;

    if (add_fd_event(sockfd, EVENT_WRITABLE, event_conn_callback, current_coro()))
        return -2;

    schedule_timeout(CONN_TIMEOUT);
    del_fd_event(sockfd, EVENT_WRITABLE);
    if (is_wakeup_by_timeout())
    {
        errno = ETIMEDOUT;
        return -3;
    }

    ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &flags, &len);
    if (ret == -1 || flags || !len)
    {
        if (flags)
            errno = flags;

        return -4;
    }

    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int connfd = 0;

    while ((connfd = g_sys_accept(sockfd, addr, addrlen)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_READABLE, event_conn_callback, current_coro()))
            return -2;

        schedule_timeout(ACCEPT_TIMEOUT);
        del_fd_event(sockfd, EVENT_READABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    if (set_nonblock(connfd))
    {
        close(connfd);
        return -4;
    }

    if (enable_tcp_no_delay(connfd))
    {
        close(connfd);
        return -5;
    }

    if (set_keep_alive(connfd, KEEP_ALIVE))
    {
        close(connfd);
        return -6;
    }

    return connfd;
}

ssize_t read(int fd, void *buf, size_t count)
{
    ssize_t n;

    while ((n = g_sys_read(fd, buf, count)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(fd, EVENT_READABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(READ_TIMEOUT);
        del_fd_event(fd, EVENT_READABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t n;

    while ((n = g_sys_readv(fd, iov, iovcnt)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(fd, EVENT_READABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(READV_TIMEOUT);
        del_fd_event(fd, EVENT_READABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    ssize_t n;

    while ((n = g_sys_recv(sockfd, buf, len, flags)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_READABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(RECV_TIMEOUT);
        del_fd_event(sockfd, EVENT_READABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                                struct sockaddr *src_addr, socklen_t *addrlen)
{
    ssize_t n;

    while ((n = g_sys_recvfrom(sockfd, buf, len, flags, src_addr, addrlen)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_READABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(RECVFROM_TIMEOUT);
        del_fd_event(sockfd, EVENT_READABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
    ssize_t n;

    while ((n = g_sys_recvmsg(sockfd, msg, flags)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_READABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(RECVMSG_TIMEOUT);
        del_fd_event(sockfd, EVENT_READABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    ssize_t n;

    while ((n = g_sys_write(fd, buf, count)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(fd, EVENT_WRITABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(WRITE_TIMEOUT);
        del_fd_event(fd, EVENT_WRITABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t n;

    while ((n = g_sys_writev(fd, iov, iovcnt)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(fd, EVENT_WRITABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(WRITEV_TIMEOUT);
        del_fd_event(fd, EVENT_WRITABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    ssize_t n;

    while ((n = g_sys_send(sockfd, buf, len, flags)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_WRITABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(SEND_TIMEOUT);
        del_fd_event(sockfd, EVENT_WRITABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                              const struct sockaddr *dest_addr, socklen_t addrlen)
{
    ssize_t n;

    while ((n = g_sys_sendto(sockfd, buf, len, flags, dest_addr, addrlen)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_WRITABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(SENDTO_TIMEOUT);
        del_fd_event(sockfd, EVENT_WRITABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
    ssize_t n;

    while ((n = g_sys_sendmsg(sockfd, msg, flags)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(sockfd, EVENT_WRITABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(SENDMSG_TIMEOUT);
        del_fd_event(sockfd, EVENT_WRITABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

ssize_t sendfile_timeout(int out_fd, int in_fd, off_t *offset, size_t count, int timeout)
{
    ssize_t n;

    while ((n = sendfile(out_fd, in_fd, offset, count)) < 0)
    {
        if (EINTR == errno)
            continue;

        if (!fd_not_ready())
            return -1;

        if (add_fd_event(out_fd, EVENT_WRITABLE, event_rw_callback, current_coro()))
            return -2;

        schedule_timeout(timeout * 1000);
        del_fd_event(out_fd, EVENT_WRITABLE);
        if (is_wakeup_by_timeout())
        {
            errno = ETIME;
            return -3;
        }
    }

    return n;
}

__attribute__((constructor)) static void syscall_hook_init()
{
    HOOK_SYSCALL(sleep);
    HOOK_SYSCALL(usleep);

    HOOK_SYSCALL(connect);
    HOOK_SYSCALL(accept);

    HOOK_SYSCALL(read);
    HOOK_SYSCALL(readv);
    HOOK_SYSCALL(recv);
    HOOK_SYSCALL(recvfrom);
    HOOK_SYSCALL(recvmsg);

    HOOK_SYSCALL(write);
    HOOK_SYSCALL(writev);
    HOOK_SYSCALL(send);
    HOOK_SYSCALL(sendto);
    HOOK_SYSCALL(sendmsg);
}

