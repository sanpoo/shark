/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>

#include "process.h"
#include "http_request.h"

int worker_process_init()
{
    int ret;

    ret = http_request_init();
    if (ret)
    {
        printf("Failed to init HTTP request. code:%d\n", ret);
        return -1;
    }

    return 0;
}

void request_handler(int fd)
{
    http_request_handler(fd);
}

PROJECT_INIT static void http_init()
{
    register_project(NULL, worker_process_init, request_handler);
}

