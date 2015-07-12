/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __CQUEUE_H__
#define __CQUEUE_H__

//circular queue

typedef void (*cqueue_rw_cb)(void *data, void *args);

struct cqueue
{
    int elem_count;         //元素个数
    size_t elem_size;       //每个元素大小
    int start;              //已用空间起始索引
    int end;                //可用空间起始索引
    cqueue_rw_cb read_cb;   //有元素可读时(消费者)的回调
    cqueue_rw_cb write_cb;  //有元素可写时(生产者)的回调
    void *elem;             //该内存由调用者申请
};

int cqueue_write(struct cqueue *queue, void *args);
int cqueue_read(struct cqueue *queue, void *args);
void cqueue_init(struct cqueue *queue, int elem_count, size_t elem_size,
                 cqueue_rw_cb read_cb, cqueue_rw_cb write_cb);

#endif
