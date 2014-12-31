/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "log.h"
#include "memcache.h"
#include "http.h"
#include "env.h"


static struct memcache *g_memc;

void handle_http_request(struct http_request *request)
{
    send_400_response(request, "this is test code");
}

/*
    注意, 在任何情况下对这个fd不要调用close, 有错误直接返回即可
*/
void handle_request(int fd)
{
    struct http_request *request = memcache_alloc(g_memc);
    if (!request)
    {
        ERR("no mem for http request");
        return;
    }

    request->fd = fd;
    if (!recv_http_request(request))
        handle_http_request(request);

    memcache_free(g_memc, request);
}

/*
    注意:
    1. 该函数还不在协程上下文里, 因此hook的接口都不能调用
    2. 成功返回0, 失败返回非0
*/
int project_init()
{
    g_memc = memcache_create(g_worker_connections, sizeof(struct http_request));
    if (!g_memc)
    {
        ERR("Failed to memcache create");
        return -1;
    }

    return 0;
}

