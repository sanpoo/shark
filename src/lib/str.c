/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include "str.h"

int str_atoi(str_t *s)
{
    int i, val = 0;

    for (i = 0; i < s->len; i++)
    {
        switch (s->p[i])
        {
            case '0' ... '9':
                val = 10 * val + (s->p[i] - '0');
                break;
            default:
                return val;
        }
    }

    return val;
}

void str2hex_lower(const char *src, int src_len, char *dest, int dest_len)
{
    int i;
    char buf[4] = {0};
    int loop = 0;

    if (dest_len < 2 * src_len)
        return;

    for (i = 0; i < src_len; i++)
    {
        sprintf(buf, "%02x", (unsigned char)src[i]);
        dest[loop++] = buf[0];
        dest[loop++] = buf[1];
    }

    dest[loop] = 0;
}

void str2hex_upper(const char *src, int src_len, char *dest, int dest_len)
{
    int i;
    char buf[4] = {0};
    int loop = 0;

    if (dest_len < 2 * src_len)
        return;

    for (i = 0; i < src_len; i++)
    {
        sprintf(buf, "%02X", (unsigned char)src[i]);
        dest[loop++] = buf[0];
        dest[loop++] = buf[1];
    }

    dest[loop] = 0;
}



