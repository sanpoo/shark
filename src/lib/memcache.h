/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __MEMCACHE_H__
#define __MEMCACHE_H__

struct memcache
{
    void **elements;    //用于挂载对象的数组
    size_t obj_size;    //对象大小
    int cache_size;     //池子大小
    int curr;           //当前可用的element个数
};

struct memcache *memcache_create(int cache_size, size_t obj_size);
void memcache_destroy(struct memcache *cache);
void *memcache_alloc(struct memcache *cache);
void memcache_free(struct memcache *cache, void *element);

#endif


