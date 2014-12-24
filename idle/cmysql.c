#include <string.h>
#include <stdlib.h>

#include "cmysql.h"
#include "log.h"

static void mysql_swap(struct SQL *s1, struct SQL *s2)
{
    MYSQL *mysql = s1->mysql;
    s1->mysql = s2->mysql;
    s2->mysql = mysql;
}

//返回0, 表示ok, 非0则失败
static inline int mysql_reconnect(struct SQL *sql)
{
    return mysql_ping(sql->mysql);
}

struct SQL *mysql_conn_get(struct conn_pool *pool)
{
    struct SQL *sql = (struct SQL *)conn_pool_get(pool);
    return sql;
}

void mysql_conn_put(struct conn_pool *pool, struct SQL *sql)
{
    conn_pool_put(pool, sql);
}

/*
    连接池是否可用(有一个连接可用就表示可用, 如果所有连接都不可用, 表示不可用)
    1:表示是活的，0:表示已经死了
    一般用作分布式系统, 测试机器是否还通
*/
int mysql_pool_alived(struct conn_pool *pool)
{
    int i;
    int isalived = 0;

    for (i = 0; i < pool->total; i++)
    {
        struct SQL *sql = mysql_conn_get(pool);
        if (sql == NULL)
            return 1;       //拿的为空, 说明全部分配出去了, 且满载或者分配失败(极少情况)

        if (mysql_reconnect(sql) == 0)   //这步返回非0, put时会收回连接
            isalived = 1;

        mysql_conn_put(pool, sql);

        if (isalived)
            return 1;
    }

    return 0;
}

static int mysql_exec_retry(struct conn_pool *pool, struct SQL *sql, const char *str, int len)
{
    int ret;
    int retry = 0;
    int errno;

    do {
        if (0 != mysql_reconnect(sql))
        {
            struct SQL *new_one = mysql_conn_get(pool);
            if (NULL == new_one)
                return SQL_ERR;

            mysql_swap(sql, new_one);
            mysql_conn_put(pool, new_one);
        }

        ret = mysql_real_query(sql->mysql, str, len);
        if (0 == ret)
            return SQL_OK;

        errno = sql_errno(sql);
    } while (++retry <= 3 &&
             (errno == CR_SERVER_GONE_ERROR ||
              errno == CR_SERVER_LOST ||
              errno == ER_QUERY_INTERRUPTED));

    return SQL_ERR;
}

int mysql_exec(struct conn_pool *pool, struct SQL *sql, const char *str)
{
    int ret;
    int len = strlen(str);
    int errno;

    ret = mysql_real_query(sql->mysql, str, len);
    if (0 == ret)
        return SQL_OK;

    errno = sql_errno(sql);

    if (errno == CR_SERVER_GONE_ERROR || errno == CR_SERVER_LOST || errno == ER_QUERY_INTERRUPTED)
        return mysql_exec_retry(pool, sql, str, len);

    if (errno == ER_DUP_ENTRY)  //主键冲突
        return SQL_DATA_EXIST;

    return SQL_STR_ERR;
}

int mysql_exec_and_parse(struct conn_pool *pool, struct SQL *sql, const char *str,
                              int (*parse)(MYSQL_ROW row, void *obj), void *obj)
{
    int ret;

    ret = mysql_exec(pool, sql, str);
    if (SQL_OK != ret)
        return ret;

    MYSQL_RES *result = NULL;
    ret = SQL_DATA_NOT_FOUND;

    do {
        result = mysql_use_result(sql->mysql);
        if (result == NULL)
            break;

        MYSQL_ROW row;
        while (NULL != (row = mysql_fetch_row(result)))
            ret = parse(row, obj);

        mysql_free_result(result);
    } while (mysql_next_result(sql->mysql) == 0);

    return ret; //要么返回SQL_DATA_NOT_FOUND, 要么就是parse返回的结果
}

/*
    返回上次UPDATE更改的行数，上次DELETE删除的行数，或上次INSERT语句插入的行数
*/
int mysql_update(struct conn_pool *pool, struct SQL *sql, const char *str, unsigned *affected)
{
    int ret = mysql_exec(pool, sql, str);

    *affected = mysql_affected_rows(sql->mysql);

    return ret;
}

/*
    以下为mysql连接池处理, 无需关注
*/
struct mysql_conf
{
    char host[64];
    char user[64];
    char passwd[64];
    unsigned int port;

    int conn_timeout;    //与mysql服务器连接超时时间. >0时才会设置, 否则永远不超时
};

static void *mysql_open_cb(void *args)
{
    struct SQL *sql = (struct SQL *)malloc(sizeof(struct SQL));
    if (NULL == sql)
        return NULL;

    sql->mysql = mysql_init(NULL);
    if (NULL == sql->mysql)
    {
        free(sql);
        ERR("Failed to init mysql.");
        return NULL;
    }

    return sql;
}

static int mysql_connect_cb(void *conn, void *args)
{
    struct SQL *sql = (struct SQL *)conn;
    struct mysql_conf *conf = (struct mysql_conf *)args;
    int reconn = 1;

    mysql_options(sql->mysql, MYSQL_OPT_RECONNECT, (char *)&reconn);

    if (conf->conn_timeout > 0)
        mysql_options(sql->mysql, MYSQL_OPT_CONNECT_TIMEOUT, (char *)&conf->conn_timeout);

    MYSQL *result = mysql_real_connect(sql->mysql, conf->host, conf->user, conf->passwd,
                                       NULL, conf->port, NULL, CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS);
    if (NULL == result)
    {
        ERR("mysql_real_connect failed. %d %s", sql_errno(sql), sql_error(sql));
        return -1;
    }

    mysql_set_character_set(sql->mysql, "utf8");

    return 0;
}

static void mysql_close_cb(void *conn)
{
    struct SQL *sql = (struct SQL *)conn;

    mysql_close(sql->mysql);
    free(sql);
}

/*
    测试是否需要关闭连接(在连接出错的情况下) 0:不需要关闭，1:需要关闭
*/
static int mysql_need_close_cb(void *conn)
{
    struct SQL *sql = (struct SQL *)conn;
    int err = mysql_errno(sql->mysql);

    if (err == CR_SERVER_GONE_ERROR ||
        err == ER_QUERY_INTERRUPTED ||
        err == CR_SERVER_LOST ||
        err == CR_UNKNOWN_ERROR ||
        err == CR_COMMANDS_OUT_OF_SYNC)
        return 1;

    return 0;
}

static struct mysql_conf *alloc_mysql_conf(const char *host, const char *user,
                        const char *passwd, unsigned int port, int conn_timeout)
{
    struct mysql_conf *conf = (struct mysql_conf *)calloc(1, sizeof(struct mysql_conf));
    if (NULL == conf)
        return NULL;

    strncpy(conf->host, host, sizeof(conf->host) - 1);
    strncpy(conf->user, user, sizeof(conf->user) - 1);
    strncpy(conf->passwd, passwd, sizeof(conf->passwd) - 1);
    conf->port = port;
    conf->conn_timeout = conn_timeout;

    return conf;
}

static inline void free_mysql_conf(struct mysql_conf *conf)
{
    free(conf);
}

/*
    conn_timeout      与mysql服务器连接超时时间. >0时才有效, 否则系统默认28800秒后(8小时)超时
    conn_idel_timeout 连接的最大空闲时间, 单位秒
*/
struct conn_pool *mysql_pool_create(const char *host, const char *user,
                        const char *passwd, unsigned int port, int conn_timeout,
                                            int conn_max, int conn_idel_timeout)
{
    struct conn_ops ops;
    struct mysql_conf *conf;

    conf = alloc_mysql_conf(host, user, passwd, port, conn_timeout);
    if (NULL == conf)
        return NULL;

    ops.conn_open_cb = mysql_open_cb;
    ops.conn_connect_cb = mysql_connect_cb;
    ops.conn_close_cb = mysql_close_cb;
    ops.conn_need_close_cb = mysql_need_close_cb;

    return conn_pool_create(0, conn_max, conn_idel_timeout, &ops, conf);
}

void mysql_pool_destroy(struct conn_pool *pool)
{
    struct mysql_conf *conf = (struct mysql_conf *)pool->args;

    free_mysql_conf(conf);
    conn_pool_destroy(pool);
}


