/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __STR_H__
#define __STR_H__

int format_args(char *str, size_t size, const char *format, ...);

int str_equal(const char *s, const char *t);
int str_case_equal(const char *s, const char *t);

char *trim_left(char *str);
char *trim_right(char *str);
char *trim(char *str);
void str2hex_lower(const char *src, int src_len, char *dest, int dest_len);
void str2hex_upper(const char *src, int src_len, char *dest, int dest_len);
const char *find_key_value(const char *str, const char *key, size_t *len);
int copy_key_value(const char *str, const char *key, char *value, size_t len);
const char *find_key_value_end(const char *str, const char *key, size_t *len);


#endif
