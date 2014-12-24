/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __SHM_H__
#define __SHM_H__

void *shm_alloc(size_t size_bytes);
void *shm_alloc_pages(unsigned int pg_count);
void shm_free_pages(void *addr, unsigned int pg_count);
void shm_init();

#endif

