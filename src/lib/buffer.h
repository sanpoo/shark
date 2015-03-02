/*
    Copyright (C) 2015 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stddef.h>

struct buffer
{
    unsigned char *start;
    unsigned char *end;
    unsigned char *pos;
    unsigned char *last;
};

void bind_buffer(struct buffer *b, void *buff, size_t len);

#endif

