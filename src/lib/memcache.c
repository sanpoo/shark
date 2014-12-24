/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdlib.h>
#include <assert.h>

#include "memcache.h"

static inline void add_element(struct memcache *cache, void *element)
{
    if (cache->curr < cache->cache_size)
    {
        cache->elements[cache->curr++] = element;
        return;
    }

    free(element);
}

static inline void *remove_element(struct memcache *cache)
{
    return cache->elements[--cache->curr];
}

static void free_pool(struct memcache *cache)
{
    while (cache->curr)
    {
        void *element = remove_element(cache);
        free(element);
    }

    free(cache->elements);
    free(cache);
}

/*
    @cache_size:最多缓存元素个数
    @obj_size:  元素对象大小
    memcache原理:
    初始化时缓存cache_size个元素, 后续分配从cache中分, 如果超过cache部分, 则malloc
    释放回来的时候, 也最多装满cache_size个元素, 多的部分free掉
    cache_size的选择应该选择大部分时候使用的量, 不要选择极端情形下的内存数字
 */
struct memcache *memcache_create(int cache_size, size_t obj_size)
{
    struct memcache *cache;

    assert(cache_size >= 1);

    cache = (struct memcache *)malloc(sizeof(struct memcache));
    if (NULL == cache)
        return NULL;

    cache->elements = malloc(cache_size * sizeof(void *));
    if (NULL == cache->elements)
    {
        free(cache);
        return NULL;
    }

    cache->obj_size = obj_size;
    cache->cache_size = cache_size;
    cache->curr = 0;

    while (cache->curr < cache->cache_size)
    {
        void *element = malloc(cache->obj_size);
        if (NULL == element)
        {
            free_pool(cache);
            return NULL;
        }

        add_element(cache, element);
    }

    return cache;
}

void memcache_destroy(struct memcache *cache)
{
    free_pool(cache);
}

void *memcache_alloc(struct memcache *cache)
{
    if (cache->curr)
        return remove_element(cache);

    return malloc(cache->obj_size);
}

void memcache_free(struct memcache *cache, void *element)
{
    add_element(cache, element);
}


