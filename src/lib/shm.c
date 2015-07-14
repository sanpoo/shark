/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "pub.h"
#include "env.h"
#include "shm.h"
#include "spinlock.h"

struct shm
{
    char *addr;     //mmap的起始指针, 初始化后永不变
    size_t size;    //mmap的大小, 初始化后永不变
    size_t offset;  //记录已经被使用的内存偏移
    spinlock lock;  //用于分配时的自旋锁
};

static struct shm *g_shm;    //全局唯一一个分配小共享内存的空间

/*
    一旦分配, 则不回收
*/
void *shm_alloc(size_t size_bytes)
{
    char *addr;

    size_bytes = roundup(size_bytes, MEM_ALIGN);

    spin_lock(&g_shm->lock);
    if (unlikely(g_shm->offset + size_bytes > g_shm->size))
    {
        spin_unlock(&g_shm->lock);
        return NULL;
    }

    addr = g_shm->addr + g_shm->offset;
    g_shm->offset += size_bytes;
    spin_unlock(&g_shm->lock);

    //printf("global share memory left bytes: %zu\n", g_shm->size - g_shm->offset);

    return addr;
}

/*
    该接口用于申请页大小的共享空间, 建议申请4K以上的均用此接口
    @pg_count  :页的个数, 最终申请大小为pg_count * PAGE_SIZE
*/
void *shm_pages_alloc(unsigned int pg_count)
{
    void *addr;

    addr = mmap(NULL, pg_count * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    return (addr == MAP_FAILED) ? NULL : addr;
}

void shm_pages_free(void *addr, unsigned int pg_count)
{
    munmap(addr, pg_count * PAGE_SIZE);
}

/*
    这是一个供进程间共享数据的内存分配, 前SHM_OFFSET个字节用于存储shm
    这种结构体分配的内存, 最好用于分配小字节, 否则请使用shm_alloc_page
*/
void shm_init()
{
    char *addr;

    addr = shm_pages_alloc(1);
    if (!addr)
    {
        printf("Failed to init system share mem\n");
        exit(0);
    }

    g_shm = (struct shm *)addr;
    g_shm->addr = addr;
    g_shm->size = PAGE_SIZE;
    g_shm->offset = roundup(sizeof(struct shm), MEM_ALIGN);
    spin_lock_init(&g_shm->lock);
}

