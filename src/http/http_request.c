/*
    Copyright (C) 2015 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "str.h"
#include "log.h"
#include "net.h"
#include "env.h"
#include "conf.h"
#include "hash.h"
#include "memcache.h"

#include "http_parse.h"
#include "http_request.h"
#include "http_response.h"

static size_t g_http_header_size;
static struct memcache *g_http_request_cache;
static struct hash_table *g_request_header_ht;

static int request_line_method_handler(struct http_request *r)
{
    return 0;
}

struct request_line_handler g_http_line_in =
{
    request_line_method_handler,
    NULL,
    NULL
};

static int http_process_content_length(struct http_request *r)
{
    r->content_len = str_atoi(&r->header_value);
    if (r->content_len <= 0)
        return -1;

    return 0;
}

static int http_process_host(struct http_request *r)
{
    size_t i, dot_pos, host_len;
    unsigned char ch;
    enum
    {
        sw_usual = 0,
        sw_literal,
        sw_rest
    } state;

    state = sw_usual;
    dot_pos = host_len = r->header_value.len;
    r->host.p = r->header_value.p;

    for (i = 0; i < r->header_value.len; i++)
    {
        ch = r->header_value.p[i];
        switch (ch)
        {
            case '.':
                if (dot_pos == i - 1)
                    return -1;
                dot_pos = i;
                break;
            case ':':
                if (state == sw_usual)
                {
                    host_len = i;
                    state = sw_rest;
                }
                break;
            case '[':
                if (i == 0)
                    state = sw_literal;
                break;
            case ']':
                if (state == sw_literal)
                {
                    host_len = i + 1;
                    state = sw_rest;
                }
                break;
            case '\0':
                return -1;
            default:
                if (ch == '/')
                    return -2;
                if (ch >= 'A' && ch <= 'Z')
                    r->header_value.p[i] = tolower(ch);
                break;
        }
    }

    if (dot_pos == host_len - 1)
        host_len--;
    if (host_len == 0)
        return -3;

    r->host.len = host_len;

    return 0;
}

struct request_header_handler g_http_headers_in[] =
{
    {STRING("Host"),                http_process_host},
    {STRING("Connection"),          NULL},
    {STRING("If-Modified-Since"),   NULL},
    {STRING("If-Unmodified-Since"), NULL},
    {STRING("If-Match"),            NULL},
    {STRING("If-None-Match"),       NULL},
    {STRING("User-Agent"),          NULL},
    {STRING("Referer"),             NULL},
    {STRING("Content-Length"),      http_process_content_length},
    {STRING("Content-Type"),        NULL},
    {STRING("Range"),               NULL},
    {STRING("If-Range"),            NULL},
    {STRING("Transfer-Encoding"),   NULL},
    {STRING("Expect"),              NULL},
    {STRING("Upgrade"),             NULL},
    {STRING("Authorization"),       NULL},
    {STRING("Keep-Alive"),          NULL},
    {STRING("Cookie"),              NULL},
    {NULL_STRING,                   NULL},
};

static int recv_http_request(int fd, struct buffer *b)
{
    int nread = -1;

    if (b->last == b->end)
        return -1;

    nread = recv(fd, b->last, b->end - b->last, 0);
    if (nread < 0)
    {
        if (ETIME == errno)
        {
            ERR("recv timeout, client ip:%s", get_peer_ip(fd));
            return -2;
        }
        else
        {
            ERR("recv http request error. ret:%d errno:%d %s", nread, errno, strerror(errno));
            return -3;
        }
    }
    else if (nread == 0)
    {
        ERR("closed by client, client ip:%s", get_peer_ip(fd));
        return -4;
    }

    b->last += nread;

    return 0;
}

static int http_process_request_line(struct http_request *request)
{
    int ret;
    struct buffer *b = &request->header;

    for (;;)
    {
        ret = recv_http_request(request->fd, b);
        if (ret)
        {
            ERR("recv http request line error:%d", ret);
            return -1;
        }

        ret = http_parse_request_line(request, b);
        if (ret == 0)       //请求行解析成功
            return 0;
        else if (ret > 0)   //需要继续读请求行
            continue;
        else                //解析出现错误
        {
            ERR("parse request line error:%d", ret);
            return -2;
        }
    }

    return 0;
}

static void handle_http_request_header(struct http_request *request)
{
    struct request_header_handler *hh;

    if (request->invalid_header)
        return;

    hh = hash_table_find(g_request_header_ht, request->header_hash);
    if (hh && hh->handler)
        hh->handler(request);
}

static int http_process_request_header(struct http_request *request)
{
    int ret;
    struct buffer *b = &request->header;

    for (;;)
    {
        ret = http_parse_request_header(request, b);
        if (ret == 0)       //请求头解析成功一个
        {
            handle_http_request_header(request);
            continue;
        }
        else if (ret == 1)  //需要继续读请求行
        {
            ret = recv_http_request(request->fd, b);
            if (ret)
            {
                ERR("recv http request header error:%d", ret);
                return -1;
            }
            continue;
        }
        else if (ret == 100)
            return 0;
        else                //ret < 0 解析出现错误
        {
            ERR("parse request header error:%d", ret);
            return -2;
        }
    }

    return 0;
}

static int http_process_request_content(struct http_request *request)
{
    int ret;
    struct buffer *b = &request->header;

    while (b->last - b->pos != request->content_len)
    {
        ret = recv_http_request(request->fd, b);
        if (ret)
        {
            ERR("recv http request content error:%d", ret);
            return -1;
        }
    }

    request->content.p = b->pos;
    request->content.len = request->content_len;

    return 0;
}

#if 0
static void print_request_line(struct http_request *request)
{
    DBG("request line [%.*s]", (int)request->request_line.len, request->request_line.p);
    DBG("method_name  [%.*s]", (int)request->method_name.len, request->method_name.p);
    DBG("method       [%d]", request->method);
    DBG("schema       [%.*s]", (int)request->schema.len, request->schema.p);
    DBG("host         [%.*s]", (int)request->host.len, request->host.p);
    DBG("port         [%.*s]", (int)request->port.len, request->port.p);
    DBG("uri          [%.*s]", (int)request->uri.len, request->uri.p);
    DBG("args         [%c]", request->args ? *request->args : '0');
    if (request->exten.p)
        DBG("exten        [%.*s]", (int)request->exten.len, request->exten.p);
    else
        DBG("exten        [NULL]");
    if (request->http_protocol.p)
        DBG("http_protocol[%.*s]", (int)request->http_protocol.len, request->http_protocol.p);
    else
        DBG("http_protocol[NULL]");
    DBG("http version   [%d]", request->http_version);
    DBG("content len:%d [%.*s]", request->content_len, request->content.len, request->content.p);
}
#endif

static void http_process_request(struct http_request *request)
{
    //print_request_line(request);
    http_finalize_request(request, 200);
}

static void __http_request_handler(struct http_request *request)
{
    if (http_process_request_line(request))
    {
        http_finalize_request(request, HTTP_BAD_REQUEST);
        return;
    }

    if (request->http_version < HTTP_VER_10)
    {
        http_process_request(request);
        return;
    }

    if (http_process_request_header(request))
    {
        http_finalize_request(request, HTTP_BAD_REQUEST);
        return;
    }

    if (http_process_request_content(request))
    {
        http_finalize_request(request, HTTP_INSUFFICIENT_STORAGE);
        return;
    }

    http_process_request(request);
}

int http_request_handler(int fd)
{
    struct http_request *request;

    request = memcache_alloc(g_http_request_cache);
    if (!request)
    {
        ERR("no mem for http request");
        http_fast_response(fd, HTTP_INSUFFICIENT_STORAGE_PAGE, sizeof(HTTP_INSUFFICIENT_STORAGE_PAGE) - 1);
        return -1;
    }

    request->fd = fd;
    request->state = 0;
    str_init(&request->request_line);
    request->args = NULL;
    str_init(&request->exten);
    str_init(&request->http_protocol);
    request->content_len = 0;

    bind_buffer(&request->header, (char *)request + sizeof(struct http_request),
                g_http_header_size - sizeof(struct http_request));

    __http_request_handler(request);
    memcache_free(g_http_request_cache, request);
    return 0;
}

int http_request_init()
{
    size_t size = 2048;
    struct request_header_handler *header;
    char *c;

    c= get_raw_conf("client_header_buffer_sizekbyte");
    if (!str_equal(c, "default"))
    {
        size = atoi(c);
        if (size <= 0 || size > 4)
        {
            ERR("client header buffer size should between [1-4]KB");
            return -1;
        }

        size <<= 10;
    }

    g_http_header_size = size;
    g_http_request_cache = memcache_create(size, g_worker_connections);
    if (!g_http_request_cache)
    {
        ERR("Failed to create memcache for HTTP request header");
        return -2;
    }

    g_request_header_ht = hash_table_create(128);
    if (!g_request_header_ht)
    {
        ERR("Failed to create hash table for HTTP request header");
        return -3;
    }

    for (header = g_http_headers_in; header->name.len; header++)
    {
        if (!header->handler)
            continue;

        unsigned key = hash_key_lc(header->name.p, header->name.len);
        if (hash_table_add(g_request_header_ht, key, header))
        {
            ERR("request header conflict. key:%u, header:%.*s", key,
                header->name.len, header->name.p);
            return -4;
        }
    }

    return 0;
}
