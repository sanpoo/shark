/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __CONN_POOL_H__
#define __CONN_POOL_H__

#include <time.h>

struct conn_ops
{
    void *(*conn_open_cb)(void *args);  //打开一个连接, 例如:socket返回fd 或 mysql_init
    int   (*conn_connect_cb)(void *conn, void *args); //连接成功返回0, 其他返回非0
    void  (*conn_close_cb)(void *conn);  //关闭一个连接, 例如:socket close, or mysql_close
    int   (*conn_need_close_cb)(void *conn); //测试是否需要关闭连接(在连接出错的情况下) 0:不需要关闭，1:需要关闭
};

struct conn_node
{
    time_t last;            //上一次, 连接加入队列的时间
    void *conn;             //调用者数据
};

struct conn_pool
{
    int min;                //最少连接个数
    int max;                //最大连接个数
    int left;               //池子里可用连接数
    int total;              //总共的连接数(池子里+已分配出去)
    int idel_timeout;       //连接最大空闲时间, 单位秒

    void *args;             //供回调函数使用的参数
    struct conn_ops ops;    //回调函数集

    struct conn_node *array;//环形数组
    int start;              //可用连接头index
    int end;                //可用连接尾index
};

void *conn_pool_get(struct conn_pool *pool);
void conn_pool_put(struct conn_pool *pool, void *conn);

void conn_pool_destroy(struct conn_pool *pool);
struct conn_pool *conn_pool_create(int min, int max, int idel_timeout,
                                        struct conn_ops *ops, void *args);

#endif

