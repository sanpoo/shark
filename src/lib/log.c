/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "shark.h"
#include "env.h"
#include "shm.h"
#include "str.h"
#include "util.h"
#include "cqueue.h"
#include "spinlock.h"
#include "sys_hook.h"
#include "log.h"

#define LOG_SIZE        (1024 * 1024)

struct log_desc
{
    char msg[1024];
    struct logger *log;
    const char *file;
    const char *func;
    int line;
    enum LOG_LEVEL level;
};

struct log_buff
{
    size_t len;                         //buff长度
    char buff[1024 - sizeof(size_t)];   //日志缓冲区
};

struct logger
{
    int pid;                //与进程绑定的pid号
    int need_init;          //进程退出时, 为1, 待日志写完后, 初始化为0
    struct cqueue queue;    //日志循环队列
};

static char STR_LOG_LEVEL[][8] = {"CRIT", "ERR", "WARN", "INFO", "DBG"};

static int logger_id;               //每个进程一个id

static int g_log_today;             //记录当前的天
static int g_log_fd;                //日志文件描述符
static enum LOG_LEVEL g_log_level;  //日志级别

static char *g_flush_page;          //磁盘的缓冲区, 一个PAGE_SIZE大小
static size_t g_flush_page_len;     //已经使用的长度

static spinlock *g_log_lock;        //用于多进程分配和释放时
static int g_logger_num;            //log的个数(master与worker统一管理)
static struct logger *g_logger;     //指向含有g_logger_num个logger的共享内存

static enum LOG_LEVEL log_str_level(const char *level)
{
    int index;

    for (index = 0; index < ARRAY_SIZE(STR_LOG_LEVEL); index++)
    {
        if (str_equal(level, STR_LOG_LEVEL[index]))
            return (enum LOG_LEVEL)index;
    }

    printf("unknow log level. %s\n", level);
    exit(0);
}

/*
    1. 重命名当前文件
    2. 新建一个文件
    3. 删掉过期日志文件
*/
static void change_log(struct tm *tp)
{
    time_t t;
    char filename[256];
    char datefmt[256];
    struct stat statfile;

    g_log_today = tp->tm_mday;

    strftime(datefmt, sizeof(datefmt), "_%Y%m%d", tp);
    sprintf(filename, "%s%s", g_log_path, datefmt);

    if (stat(g_log_path, &statfile) == 0)
        rename(g_log_path, filename);

    close(g_log_fd);
    g_log_fd = open(g_log_path, O_CREAT | O_RDWR | O_APPEND, 0644);

    time(&t);
    t -= g_log_reserve_days * 24 * 3600;
    tp = localtime(&t);
    strftime(datefmt, sizeof(datefmt), "_%Y%m%d", tp);
    sprintf(filename, "%s%s", g_log_path, datefmt);

    if (stat(filename, &statfile) == 0)
        remove(filename);
}

static void log_flush()
{
    struct tm *tp;

    if (0 == g_flush_page_len)
        return;

    tp = get_tm();
    if (unlikely(tp->tm_mday != g_log_today))
        change_log(tp);

    if (likely(g_log_fd > 0))
        g_sys_write(g_log_fd, g_flush_page, g_flush_page_len);
    g_flush_page_len = 0;
}

static void log_read(void *data, void *args)
{
    struct log_buff *buff = data;

    if (buff->len > PAGE_SIZE - g_flush_page_len)
        log_flush();

    memcpy(g_flush_page + g_flush_page_len, buff->buff, buff->len);
    g_flush_page_len += buff->len;
}

static void log_write(void *data, void *args)
{
    struct log_buff *buff = data;
    struct log_desc *desc = args;
    struct tm *tp = get_tm();
    char strnow[64];

    strftime(strnow, sizeof(strnow), "%Y-%m-%d %H:%M:%S", tp);
    buff->len = snprintf(buff->buff, sizeof(buff->buff),
                         "(%d)%s %s %s(%d)[%s]:%s\n",
                         desc->log->pid, strnow, desc->file,
                         desc->func, desc->line,
                         STR_LOG_LEVEL[desc->level], desc->msg);
}

static inline void copy_to_cache(struct logger *log)
{
    while (-1 != cqueue_read(&log->queue, NULL))
        ;

    if (1 == log->need_init)
    {
        log->pid = 0;
        log->need_init = 0;
    }
}

/*
    扫描日志的共享内存, 并写到文件
    满一页就往磁盘写, 扫完一轮后, 不够一页也写
*/
void log_scan_write()
{
    int index;

    for (index = 0; index < g_logger_num; index++)
    {
        if (g_logger[index].pid == 0)
            continue;

        copy_to_cache(&g_logger[index]);
    }

    log_flush();
}

/*
    成功返回0, 失败返回<0
*/
int log_worker_alloc(int pid)
{
    int index;
    struct logger *log;

    spin_lock(g_log_lock);
    for (index = 0; index < g_logger_num; index++)
    {
        log = &g_logger[index];

        if (log->pid == 0)
        {
            log->pid = pid;
            log->need_init = 0;
            cqueue_init(&log->queue, LOG_SIZE / sizeof(struct log_buff),
                        sizeof(struct log_buff), log_read, log_write);

            logger_id = index;
            spin_unlock(g_log_lock);
            return 0;
        }
    }

    spin_unlock(g_log_lock);

    return -1;
}

//worker进程退出的时候, 记得加上这句
void log_worker_flush_and_reset(int pid)
{
    int index;

    spin_lock(g_log_lock);
    for (index = 0; index < g_logger_num; index++)
    {
        if (g_logger[index].pid == pid)
            g_logger[index].need_init = 1;
    }
    spin_unlock(g_log_lock);
}

struct log_desc g_desc;

void log_out(enum LOG_LEVEL level, const char *file, const char *func,
             int line, const char *fmt, ...)
{
    int ret;

    if (level > g_log_level)
        return;

    g_desc.log = &g_logger[logger_id];
    g_desc.file = file;
    g_desc.func = func;
    g_desc.line = line;
    g_desc.level = level;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_desc.msg, sizeof(g_desc.msg), fmt, ap);
    va_end(ap);

    ret = cqueue_write(&g_desc.log->queue, &g_desc);
    //if (unlikely(ret))
    //printf("id:%d log buff is full\n", logger_id);
}

static int gen_logfile_fd(const char *path)
{
    int fd;

    if (access(path, F_OK | R_OK | W_OK) == 0)
        fd = open(path, O_RDWR | O_APPEND, 0644);
    else
        fd = open(path, O_CREAT | O_RDWR | O_APPEND, 0644);

    if (fd == -1)
    {
        printf("%s -> %s\n", strerror(errno), path);
        exit(0);
    }

    return fd;
}

void log_init()
{
    int index = 0;

    g_log_today = get_tm()->tm_mday;
    g_log_fd = gen_logfile_fd(g_log_path);
    g_log_level = log_str_level(g_log_strlevel);
    g_flush_page = (char *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (!g_flush_page)
    {
        printf("Failed to alloc page for log flush\n");
        exit(0);
    }

    g_flush_page_len = 0;
    g_log_lock = shm_alloc(sizeof(spinlock));
    spin_lock_init(g_log_lock);

    g_logger_num = g_worker_processes + 1; //include master process
    g_logger = shm_alloc(sizeof(struct logger) * g_logger_num);
    if (!g_logger)
    {
        printf("Failed to alloc mem for logger\n");
        exit(0);
    }

    for (index = 0; index < g_logger_num; index++)
    {
        g_logger[index].pid = 0;
        g_logger[index].queue.elem = shm_pages_alloc(LOG_SIZE / PAGE_SIZE);
        if (!g_logger[index].queue.elem)
        {
            printf("Failed to alloc pages for each logger\n");
            exit(0);
        }
    }
}

