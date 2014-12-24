/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __HTTP_H__
#define __HTTP_H__

#include "refstr.h"

#define HTTP_BUFF_SIZE    2048

struct http_request
{
    char buff[HTTP_BUFF_SIZE];
    int fd;
    int header_len;
    int content_len;
    struct refstr path;
    struct refstr cookie;
};

int recv_http_request(struct http_request *request);
void send_http_response_200(struct http_request *request,
                                    char *buff, size_t buff_len,
                                    char *content, size_t content_len);
void send_404_response(struct http_request *request);
void send_400_response(struct http_request *request, const char *reason);

#endif

