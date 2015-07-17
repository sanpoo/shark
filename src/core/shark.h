/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __SHARK_H__
#define __SHARK_H__

#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

#define MEM_ALIGN   sizeof(unsigned long)

#define ALIGN_MASK(x,mask)  (((x)+(mask))&~(mask))
#define ALIGN(x,a)		    ALIGN_MASK(x, (typeof(x))(a)-1)
#define DIV_ROUND_UP(n,d)   (((n) + (d) - 1) / (d))
#define roundup(x, y)       ((((x) + ((y) - 1)) / (y)) * (y))

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))

#endif


