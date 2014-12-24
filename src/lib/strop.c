/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

int format_args(char *str, size_t size, const char *format, ...)
{
    int len;

    va_list ap;
    va_start(ap, format);
    len = vsnprintf(str, size, format, ap);
    va_end(ap);

    return len;
}

int str_equal(const char *s, const char *t)
{
    return !strcmp(s, t);
}

int str_case_equal(const char *s, const char *t)
{
    return !strcasecmp(s, t);
}

/*
    去掉以\0结尾的字符串最左边的空格, 不可见字符
 */
char *trim_left(char *str)
{
    char *loop = str;

    while (*loop != '\0')
    {
        if (!isspace(*loop) && isprint(*loop))
            break;

        loop++;
    }

    memmove(str, loop, strlen(loop) + 1);

    return str;
}

/*
    去掉以\0结尾的字符串最右边的空格, 不可见字符
 */
char *trim_right(char *str)
{
    char *loop = str + strlen(str) - 1;

    while (loop >= str)
    {
        if (!isspace(*loop) && isprint(*loop))
            break;

        loop--;
    }

    *(++loop) = '\0';

    return str;
}

/*
    去掉以\0结尾的字符串左右两边的空格, 不可见字符
 */
char *trim(char *str)
{
    str = trim_left(str);
    return trim_right(str);
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

/*
    find_key_value find the first key's value from str, found string indicated by
    return value and args len

    RETURN VALUE:
    if find key, and the value is not empty then return the first ptr, len by args

    NOTES:
    1. function only find the first key, if str have more then one key,
       the left will be ignored
    2. function will not copy any char
    3. caller should make sure the str and key are not null

    EXAMPLE:
    find_key_value("Version 5.20, Release 5101L06", "Version", ...)  ->5.20
    find_key_value("Version 5.20, Release 5101L06", "Release", ...)  ->5101L06
    find_key_value("type:123, this:::", "type", ...)                 ->123
    find_key_value("type:123, this:::", "this", ...)                 ->empty str
    find_key_value("Patch    : P001", "Patch", ...)                  ->P001
    find_key_value("PID: CL,VID: , SN: SMT0847B906", "VID", ...)     ->empty str
*/
const char *find_key_value(const char *str, const char *key, size_t *len)
{
    const char *ptr, *loop;

    loop = strstr(str, key);
    if (loop == NULL)
        return NULL;

    loop += strlen(key);

    while (1)
    {
        if (*loop == '\n' || *loop == '\0')
            return NULL;

        if (*loop != ' ' && *loop != ':')
            break;

        loop++;
    }

    ptr = loop;

    while (1)
    {
        if (*loop == ' ' || *loop == ',' || *loop == ';' || *loop == '\n' || !isprint(*loop))
            break;

        loop++;
    }

    return (0 == (*len = loop - ptr)) ? NULL : ptr;
}

/*
    copy_key_value find the first key's value, and copy it to value

    NOTES:
    1. caller should make sure the str and key is not null

    RETURN VALUE:
    if find key, and the value is not empty then return the bytes we copy
*/
int copy_key_value(const char *str, const char *key, char *value, size_t len)
{
    size_t value_len;
    const char *ptr;

    ptr = find_key_value(str, key, &value_len);
    if (NULL == ptr)
        return 0;

    if (value_len >= len)
        return 0;

    memcpy(value, ptr, value_len);
    value[value_len] = 0;

    return value_len;
}

/*
    find_key_value_end find the first key, and then return the left str in line
    not include \n

    return value
    if find key, and the left str is not empty then return the first ptr, len by args

    NOTES
    1. function only find the first key, if str have more then one key,
       the left will be ignore
    2. function will not copy any char
    3. caller should make sure the str and key is not null
    4. not like copy_key_value, it find to the line end
    5. find_key_value_end will not fliter any begining characters
*/
const char *find_key_value_end(const char *str, const char *key, size_t *len)
{
    const char *ptr, *loop;

    loop = strstr(str, key);
    if (loop == NULL)
        return NULL;

    loop += strlen(key);
    ptr = loop;

    while (1)
    {
        if (*loop == '\r' || *loop == '\n' || !isprint(*loop))
            break;

        loop++;
    }

    return (0 == (*len = loop - ptr)) ? NULL : ptr;
}


