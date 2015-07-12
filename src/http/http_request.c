/*
    Copyright (C) 2015 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "str.h"
#include "log.h"
#include "net.h"
#include "env.h"
#include "hash.h"
#include "memcache.h"

#include "http_parse.h"
#include "http_request.h"
#include "http_response.h"

static size_t g_client_header_size;
static struct memcache *g_http_request_cache;

static struct request_line_handler g_request_line_handler = {NULL, NULL, NULL};
static struct hash_table *g_request_header_ht;

#if 1
static void print_request_line(struct http_request *r)
{
    DBG("r line [%.*s]", (int)r->request_line.len, r->request_line.p);
    DBG("method_name  [%.*s]", (int)r->method_name.len, r->method_name.p);
    DBG("method       [%d]", r->method);
    DBG("schema       [%.*s]", (int)r->schema.len, r->schema.p);
    DBG("host         [%.*s]", (int)r->host.len, r->host.p);
    DBG("port         [%.*s]", (int)r->port.len, r->port.p);
    DBG("uri          [%.*s]", (int)r->uri.len, r->uri.p);
    DBG("args         [%c]", r->args ? *r->args : '0');
    if (r->exten.p)
        DBG("exten        [%.*s]", (int)r->exten.len, r->exten.p);
    else
        DBG("exten        [NULL]");
    if (r->http_protocol.p)
        DBG("http_protocol[%.*s]", (int)r->http_protocol.len, r->http_protocol.p);
    else
        DBG("http_protocol[NULL]");
    DBG("http version   [%d]", r->http_version);
    DBG("content len:%d [%.*s]", r->content_len, r->content.len, r->content.p);
}
#endif

static void http_request_body_default_handler(struct http_request *r)
{
    print_request_line(r);
    http_finalize_request(r, 200);
}

static http_request_body_handler g_http_request_body_handler = http_request_body_default_handler;

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

/*
    Define this var, must be end of NULL_STRING
*/
struct request_header_handler g_http_headers_handler[] =
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
            ERR("recv http request error:%d errno:%d %s", nread, errno, strerror(errno));
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

static int http_process_request_line(struct http_request *r)
{
    int ret;
    struct buffer *b = &r->header;

    for (;;)
    {
        ret = recv_http_request(r->fd, b);
        if (ret)
        {
            ERR("recv http request line error:%d", ret);
            return -1;
        }

        ret = http_parse_request_line(r, b);
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
}

static int handle_http_request_header(struct http_request *r)
{
    struct request_header_handler *hh;

    if (r->invalid_header)
        return 0;

    hh = hash_table_find(g_request_header_ht, r->header_hash);
    if (hh && hh->handler)
        return hh->handler(r);

    return 0;
}

static int http_process_request_header(struct http_request *r)
{
    int ret;
    struct buffer *b = &r->header;

    for (;;)
    {
        ret = http_parse_request_header(r, b);
        if (ret == 0)       //请求头解析成功一个
        {
            if (handle_http_request_header(r))
                return -1;

            continue;
        }
        else if (ret == 1)  //需要继续读请求行
        {
            ret = recv_http_request(r->fd, b);
            if (ret)
            {
                ERR("recv http request header error:%d", ret);
                return -2;
            }
            continue;
        }
        else if (ret == 100)
            return 0;
        else                //ret < 0 解析出现错误
        {
            ERR("parse request header error:%d", ret);
            return -3;
        }
    }
}

static int http_process_request_body(struct http_request *r)
{
    int ret;
    struct buffer *b = &r->header;

    while (b->last - b->pos != r->content_len)
    {
        ret = recv_http_request(r->fd, b);
        if (ret)
        {
            ERR("recv http request content error:%d", ret);
            return -1;
        }
    }

    r->content.p = b->pos;
    r->content.len = r->content_len;

    return 0;
}

/*
    返回错误码, 如果是系统的错误, 则由shark返回, 其他的则由用户返回
*/
static void __http_request_handler(struct http_request *r)
{
    if (http_process_request_line(r))
    {
        http_finalize_request(r, HTTP_BAD_REQUEST);
        return;
    }

    if (g_request_line_handler.method && g_request_line_handler.method(r))
        return;

    if (g_request_line_handler.uri && g_request_line_handler.uri(r))
        return;

    if (g_request_line_handler.http && g_request_line_handler.http(r))
        return;

    if (r->http_version < HTTP_VER_10)
    {
        g_http_request_body_handler(r);
        return;
    }

    if (http_process_request_header(r))
    {
        http_finalize_request(r, HTTP_BAD_REQUEST);
        return;
    }

    if (http_process_request_body(r))
    {
        http_finalize_request(r, HTTP_INSUFFICIENT_STORAGE);
        return;
    }

    g_http_request_body_handler(r);
}

void http_request_handler(int fd)
{
    struct http_request *r;

    r = memcache_alloc(g_http_request_cache);
    if (!r)
    {
        ERR("no mem for http request");
        http_fast_response(fd, HTTP_INSUFFICIENT_STORAGE_PAGE,
                           sizeof(HTTP_INSUFFICIENT_STORAGE_PAGE) - 1);
        return;
    }

    r->fd = fd;
    r->state = 0;
    str_init(&r->request_line);
    r->args = NULL;
    str_init(&r->exten);
    str_init(&r->http_protocol);
    r->content_len = 0;

    bind_buffer(&r->header, (char *)r + sizeof(struct http_request),
                g_client_header_size - sizeof(struct http_request));

    __http_request_handler(r);
    memcache_free(g_http_request_cache, r);
}

static void add_client_header_handler(struct request_header_handler *handler)
{
    unsigned key;

    if (!handler)
        return;

    for (; handler->name.len; handler++)
    {
        if (!handler->handler)
            continue;

        key = hash_key_lc(handler->name.p, handler->name.len);
        if (hash_table_add(g_request_header_ht, key, handler))
        {
            ERR("r handler conflict. key:%u, handler:%.*s", key,
                handler->name.len, handler->name.p);
            exit(-1);
        }
    }
}

void http_request_init(size_t client_header_buffer_kbytes,
                       struct request_line_handler *line_handler,
                       struct request_header_handler *header_handler,
                       http_request_body_handler body_handler)
{
    g_client_header_size = client_header_buffer_kbytes << 10;

    if (line_handler)
        memcpy(&g_request_line_handler, line_handler, sizeof(struct request_line_handler));

    g_http_request_cache = memcache_create(g_client_header_size, g_worker_connections);
    if (!g_http_request_cache)
    {
        ERR("Failed to create memcache for HTTP request header");
        exit(-1);
    }

    g_request_header_ht = hash_table_create(128);
    if (!g_request_header_ht)
    {
        ERR("Failed to create hash table for HTTP request header");
        exit(-2);
    }

    add_client_header_handler(g_http_headers_handler);
    add_client_header_handler(header_handler);
    if (body_handler)
        g_http_request_body_handler = body_handler;
}
