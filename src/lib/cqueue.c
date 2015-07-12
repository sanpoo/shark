/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stddef.h>
#include "cqueue.h"

static inline int cqueue_is_empty(struct cqueue *queue)
{
    return queue->start == queue->end;
}

static inline int cqueue_is_full(struct cqueue *queue)
{
    return (queue->end + 1) % queue->elem_count == queue->start;
}

/*
    生产者
*/
int cqueue_write(struct cqueue *queue, void *args)
{
    void *data;

    if (cqueue_is_full(queue))
        return -1;

    data = (char *)queue->elem + queue->end * queue->elem_size;
    queue->write_cb(data, args);
    queue->end = (queue->end + 1) % queue->elem_count;

    return 0;
}

/*
    消费者
*/
int cqueue_read(struct cqueue *queue, void *args)
{
    void *data;

    if (cqueue_is_empty(queue))
        return -1;

    data = (char *)queue->elem + queue->start * queue->elem_size;
    queue->read_cb(data, args);
    queue->start = (queue->start + 1) % queue->elem_count;

    return 0;
}

/*
    初始化一个环形队列
    @elem       :指向一块连续的缓冲区, 大小为elem_size * elem_count
    @elem_count :缓冲区元素个数
    @elem_size  :缓冲区里每个元素大小, 该大小最好为2的指数倍, 能提高性能
    注意:
    1) queue内存和elem内存均需要在调用前申请好
    2) 环形队列可使用的量为elem_count - 1个, 原因是要留一个作为满或者空的标识
*/
void cqueue_init(struct cqueue *queue, int elem_count, size_t elem_size,
                 cqueue_rw_cb read_cb, cqueue_rw_cb write_cb)
{
    queue->elem_count = elem_count;
    queue->elem_size = elem_size;
    queue->start = 0;
    queue->end = 0;
    queue->read_cb = read_cb;
    queue->write_cb = write_cb;
}

