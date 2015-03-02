/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __STR_H__
#define __STR_H__

#include <string.h>

#define CR     (unsigned char)13
#define LF     (unsigned char)10
#define CRLF   "\x0d\x0a"


typedef struct
{
    unsigned char *p;
    size_t len;
} str_t;

#define STRING(str)     {(unsigned char *)str, sizeof(str) - 1}
#define NULL_STRING     {NULL, 0}
#define str_set(str, text)   (str)->data = (unsigned char *)text; (str)->len = sizeof(text) - 1

static inline int str_empty(str_t *str)
{
    return !str->p || str->len == 0;
}

static inline void str_init(str_t *str)
{
    str->p = NULL;
    str->len = 0;
}

static inline int str_equal(const char *s, const char *t)
{
    return !strcmp(s, t);
}

static inline int str_case_equal(const char *s, const char *t)
{
    return !strcasecmp(s, t);
}

int str_atoi(str_t *s);
void str2hex_lower(const char *src, int src_len, char *dest, int dest_len);
void str2hex_upper(const char *src, int src_len, char *dest, int dest_len);


#endif
