/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "conf.h"
#include "process.h"
#include "http_request.h"

int worker_process_init()
{
    size_t size = 2;
    char *c = get_raw_conf("client_header_buffer_kbytes");
    if (!str_equal(c, "default"))
    {
        size = atoi(c);
        if (size <= 0 || size > 4)
        {
            ERR("client header buffer size should between [1-4]KB");
            return -1;
        }
    }

    http_request_init(size, NULL, NULL, NULL);
    return 0;
}

__INIT__ static void http_init()
{
    register_project(NULL, worker_process_init, http_request_handler);
}

