/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __PUB_H__
#define __PUB_H__

#include <stddef.h>

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))
#define ALIGN(x,a)		    ALIGN_MASK(x,(typeof(x))(a)-1)
#define DIV_ROUND_UP(n,d)   (((n) + (d) - 1) / (d))
#define roundup(x, y)       ((((x) + ((y) - 1)) / (y)) * (y))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define container_of(ptr, type, member) ({			\
        const typeof(((type *)0)->member) *__mptr = (ptr);	\
        (type *)((char *)__mptr - offsetof(type,member));})

#if (__i386__ || __i386 || __amd64__ || __amd64)
    #define cpu_pause() __asm__("pause")
#else
    #define cpu_pause()
#endif

#define CR     (u_char) 13
#define LF     (u_char) 10
#define CRLF   "\x0d\x0a"


#endif

