/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __LOG_H__
#define __LOG_H__

enum LOG_LEVEL
{
    LEVEL_CRIT  = 0,
    LEVEL_ERR   = 1,
    LEVEL_WARN  = 2,
    LEVEL_INFO  = 3,
    LEVEL_DBG   = 4
};

#define CRIT(fmt, ...)   log_out(LEVEL_CRIT, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define ERR(fmt, ...)    log_out(LEVEL_ERR,  __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define WARN(fmt, ...)   log_out(LEVEL_WARN, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define INFO(fmt, ...)   log_out(LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define DBG(fmt, ...)    log_out(LEVEL_DBG,  __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

int log_worker_alloc(int id);
void log_worker_flush_and_reset(int id);
void log_scan_write();
void log_out(enum LOG_LEVEL level, const char *file, const char *func, int line, const char *fmt, ...);
void log_init();

#endif

