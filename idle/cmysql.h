#ifndef __MYSQL_POOL_H__
#define __MYSQL_POOL_H__

#include <mysql.h>
#include <errmsg.h>
#include <mysqld_error.h>

#include "conn_pool.h"

#define SQL_OK              0
#define SQL_ERR             -1000    //sql异常(比如连接异常)(非sql语句问题引起的异常)
#define SQL_STR_ERR         -1001    //sql 执行异常(指sql语句有问题).
#define SQL_DATA_EXIST      -1010    //数据已经存在(插入数据时，出错)
#define SQL_DATA_NOT_FOUND  -1011    //查询无结果.

struct SQL
{
    MYSQL *mysql;
};

static inline int sql_errno(struct SQL *sql)
{
    return mysql_errno(sql->mysql);
}

static inline const char *sql_error(struct SQL *sql)
{
    return mysql_error(sql->mysql);
}

static inline int mysql_get_lastid(struct SQL *sql)
{
    return (int)mysql_insert_id(sql->mysql);
}

static inline int mysql_trans_begin(struct SQL *sql)
{
    return mysql_query(sql->mysql, "begin");
}

static inline int mysql_trans_commit(struct SQL *sql)
{
    return mysql_query(sql->mysql, "commit");
}

static inline int mysql_trans_rollback(struct SQL *sql)
{
    return mysql_query(sql->mysql, "rollback");
}

struct SQL *mysql_conn_get(struct conn_pool *pool);
void mysql_conn_put(struct conn_pool *pool, struct SQL *sql);
int mysql_pool_alived(struct conn_pool *pool);
int mysql_exec(struct conn_pool *pool, struct SQL *sql, const char *str);
int mysql_exec_and_parse(struct conn_pool *pool, struct SQL *sql, const char *str,
                              int (*parse)(MYSQL_ROW row, void *obj), void *obj);
int mysql_update(struct conn_pool *pool, struct SQL *sql, const char *str, unsigned *affected);

struct conn_pool *mysql_pool_create(const char *host, const char *user,
                        const char *passwd, unsigned int port, int conn_timeout,
                                           int conn_max, int conn_idel_timeout);
void mysql_pool_destroy(struct conn_pool *pool);

#endif

