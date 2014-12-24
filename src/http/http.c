/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "strop.h"
#include "net.h"
#include "log.h"
#include "http.h"

#define SHARK_HTTP_VER              "shark/1.0.1"
#define HTTP_CONTENT_STR            "Content-Length:"
#define HEADER_IF_MODIFIED_SINCE    "If-Modified-Since: "
#define RFC1123_DATE_FMT            "%a, %d %b %Y %H:%M:%S %Z"

#define header_404 "HTTP/1.1 404 Not Found\r\nServer: "SHARK_HTTP_VER"\r\n" \
                   "Content-Type: text/html\r\nConnection: Close\r\n\r\nNot found"
#define header_400 "HTTP/1.1 400 Bad Request\r\nServer: "SHARK_HTTP_VER"\r\n" \
                   "Content-Type: text/html\r\nConnection: Close\r\n\r\nBad request:%s"
#define header_200_start "HTTP/1.1 200 OK\r\nServer: "SHARK_HTTP_VER"\r\n" \
                         "Content-Type: text/html\r\nConnection: Close\r\nok"
#define header_304_start "HTTP/1.1 304 Not Modified\r\nServer: "SHARK_HTTP_VER"\r\n" \
                         "Content-Type: text/html\r\nConnection: Close\r\n"
#define header_200 "HTTP/1.1 200 OK\r\nServer: "SHARK_HTTP_VER"\r\n" \
                   "Content-Type: application/json;charset=UTF-8\r\nContent-Length: %zu\r\n" \
                   "Date: %s\r\nConnection: close\r\n\r\n%s"
#define header_200_raw "HTTP/1.1 200 OK\r\nServer: "SHARK_HTTP_VER"\r\n" \
                       "Content-Type: application/json;charset=UTF-8\r\nContent-Length: %zu\r\n" \
                       "Date: %s\r\nConnection: keep-alive\r\n\r\n"

static char* make_rfc1123_date()
{
    static char current_time[64] = { 0 };
    struct tm current_tm;
    struct tm *p_current_tm;
    time_t tm_now = time(NULL);

    p_current_tm = gmtime_r(&tm_now, &current_tm);
    strftime(current_time, sizeof(current_time), RFC1123_DATE_FMT, p_current_tm);
    return current_time;
}

static int get_content_len(char *buff)
{
    char *pos = strstr(buff, HTTP_CONTENT_STR);
    if (!pos)
        return 0;

    pos += strlen(HTTP_CONTENT_STR);
    return atoi(pos);
}

static int check_header(char **pos, struct http_request *request, int buff_len)
{
    char *tmp;

    if (*pos == request->buff)
    {
        tmp = strstr(*pos, "\r");
        if (!tmp)
            return -1;

        *pos = tmp;
    }

    tmp = strstr(*pos, "\r\n\r\n");
    if (!tmp)
        return -1;

    request->content_len = get_content_len(request->buff);
    request->header_len = tmp + strlen("\r\n\r\n") - request->buff;
    return 0;
}

/*
    POST /switchsvr HTTP/1.1
    Host: ctrl.dcm.sandai.net
    Content-Type: application/json;charset=UTF-8
    Content-Length: 272
    Cookie: productid=1; version=7.9.29.4853; peerid=14FEB5E0679B8CNQ; uid=0; sessionid=; num=83
    Connection: close

    content detail
*/
static void parse_http_request(struct http_request *request)
{
    request->path.p = find_key_value(request->buff, "/", &request->path.len);
    request->cookie.p = find_key_value_end(request->buff, "Cookie: ", &request->cookie.len);
}

int recv_http_request(struct http_request *request)
{
    int read_pos = 0;
    ssize_t nread = -1;
    int status = -1;
    char *pos = request->buff;

    request->header_len = 0;
    request->content_len = 0;

    for (;;)
    {
        nread = recv(request->fd, request->buff + read_pos, HTTP_BUFF_SIZE - 1 - read_pos, 0);
        if (nread < 0)
        {
            if (ETIME == errno)
            {
                ERR("recv timeout, ip:%s", get_peer_ip(request->fd));
                return -1;
            }
            else
            {
                ERR("recv http request error. ret:%d errno:%d %s", nread, errno, strerror(errno));
                return -2;
            }
        }
        else if (nread == 0)
        {
            ERR("closed by client, ip:%s", get_peer_ip(request->fd));
            return -3;
        }

        read_pos += nread;
        request->buff[read_pos] = 0;

        if (status == -1)
            status = check_header(&pos, request, read_pos);

        if (request->header_len + request->content_len == read_pos)
            break;
    }

    parse_http_request(request);

    return 0;
}

void send_http_response_200(struct http_request *request,
                                    char *buff, size_t buff_len,
                                    char *content, size_t content_len)
{
    size_t printed = snprintf(buff, buff_len, header_200_raw,
                              content_len, make_rfc1123_date());

    if (printed + content_len > buff_len)
    {
        ERR("buff is not enough. total:%zu, but now:%zu", printed + content_len, buff_len);
        return;
    }

    memmove(buff + printed, content, content_len);
    send(request->fd, buff, printed + content_len, 0);
}

void send_404_response(struct http_request *request)
{
    memmove(request->buff, header_404, sizeof(header_404));
    send(request->fd, request->buff, sizeof(header_404), 0);
}

void send_400_response(struct http_request *request, const char *reason)
{
    size_t printed = snprintf(request->buff, sizeof(request->buff), header_400, reason);
    send(request->fd, request->buff, printed, 0);
}

