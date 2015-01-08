/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conn_pool.h"

#define conn_pool_is_empty(pool)    (pool->left == 0)
#define conn_pool_is_full(pool)     (pool->total >= pool->max)

static void *conn_open_and_connect(struct conn_ops *ops, void *args)
{
    void *conn = ops->conn_open_cb(args);
    if (NULL == conn)
        return NULL;

    if (0 != ops->conn_connect_cb(conn, args))
    {
        ops->conn_close_cb(conn);
        return NULL;
    }

    return conn;
}

static inline void conn_close(struct conn_ops *ops, void *conn)
{
    ops->conn_close_cb(conn);
}

void *conn_pool_get(struct conn_pool *pool)
{
    void *conn;

    if (!conn_pool_is_empty(pool))
    {
        struct conn_node *start_node = &pool->array[pool->start];
        pool->start = (pool->start + 1) % pool->max;
        pool->left--;

        return start_node->conn;
    }

    if (conn_pool_is_full(pool))
        return NULL;

    conn = conn_open_and_connect(&pool->ops, pool->args);
    if (NULL == conn)
        return NULL;

    pool->total++;

    return conn;
}

void conn_pool_put(struct conn_pool *pool, void *conn)
{
    struct conn_node *start_node, *end_node;
    time_t now;

    if (pool->ops.conn_need_close_cb(conn) || conn_pool_is_full(pool))
    {
        pool->total--;
        conn_close(&pool->ops, conn);

        return;
    }

    now = time(NULL);
    start_node = &pool->array[pool->start];

    //超时且可用个数已经超过min值, 且池子里还有
    if (now - start_node->last >= pool->idel_timeout &&
        pool->total > pool->min && !conn_pool_is_empty(pool))
    {
        pool->total--;
        conn_close(&pool->ops, conn);

        return;
    }

    //回到池子
    end_node = &pool->array[pool->end];
    end_node->conn = conn;
    end_node->last = now;
    pool->end = (pool->end + 1) % pool->max;
    pool->left++;
}

/*
    调用前, 务必释放(conn_pool_put)了所有连接(即放回池子), 并且不要再调用get/put了
 */
void conn_pool_destroy(struct conn_pool *pool)
{
    struct conn_node *node;

    while (--pool->left >= 0)
    {
        node = &pool->array[pool->start];
        conn_close(&pool->ops, node->conn);

        pool->start = (pool->start + 1) % pool->max;
    }

    free(pool->array);
    free(pool);
}


/*
    @idel_timeout 连接的最大空闲时间, 单位秒, 也就是说, 如果一条连接空闲超过timeout,
                  那么连接池会将其释放掉
    @args   该参数在分配或者连接的时候需要, 即便连接池创建完成, 该内存也不能被释放
            后期可能还会跟进这个参数创建一些连接
    note:   不要用在多线程的并发场景下, 数据没有加锁, 需要多线程场景, 请联系sanpoos@gmail.com
 */
struct conn_pool *conn_pool_create(int min, int max, int idel_timeout,
                                        struct conn_ops *ops, void *args)
{
    int i;
    struct conn_pool *pool;

    if (min < 0 || min > max || idel_timeout <= 0 || ops == NULL)
    {
        printf("conn pool args illegal\n");
        return NULL;
    }

    pool = (struct conn_pool *)malloc(sizeof(struct conn_pool));
    if (NULL == pool)
    {
        printf("Failed to alloc conn pool mem\n");
        return NULL;
    }

    pool->array = (struct conn_node *)malloc(max * sizeof(struct conn_node));
    if (NULL == pool->array)
    {
        printf("Failed to alloc conn node mem\n");
        free(pool);
        return NULL;
    }

    pool->min = min;
    pool->max = max;
    pool->left = 0;
    pool->total = 0;
    pool->idel_timeout = idel_timeout;
    pool->args = args;
    memcpy(&pool->ops, ops, sizeof(struct conn_ops));
    pool->start = 0;
    pool->end = 0;

    for (i = 0; i < min; i++)
    {
        void *conn = conn_open_and_connect(&pool->ops, args);
        if (NULL == conn)
        {
            conn_pool_destroy(pool);
            return NULL;
        }

        pool->total++;
        conn_pool_put(pool, conn);
    }


    return pool;
}

