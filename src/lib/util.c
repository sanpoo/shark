/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "pub.h"
#include "env.h"
#include "util.h"

int log2n(size_t n)
{
    int i = 0;

    while ((n = n >> 1))
        i++;

    return i;
}

int log_page_order(size_t n)
{
    return log2n(n / PAGE_SIZE);
}

struct tm *get_tm()
{
    time_t t;

    time(&t);
    return localtime(&t);
}

int bind_cpu(int cpuid)
{
    cpu_set_t mask;

    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);  /* CPU_SET sets only the bit corresponding to cpu. */

    if (sched_setaffinity(0, sizeof(mask), &mask) == -1)
        return -1;

    return 0;
}

void sys_daemon()
{
    if (fork() > 0)
        exit(0);
}

void sys_res_init()
{
    struct rlimit rlim, rlim_new;

    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
    {
        rlim_new.rlim_cur = rlim_new.rlim_max = 100000;
        if (setrlimit(RLIMIT_NOFILE, &rlim_new) != 0)
        {
            printf("Failed to set rlimit file. exit!\n");
            exit(0);
        }
    }

    if (getrlimit(RLIMIT_CORE, &rlim) == 0)
    {
        rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
        if (setrlimit(RLIMIT_CORE, &rlim_new) != 0)
        {
            printf("Failed to set rlimit core. exit!\n");
            exit(0);
        }
    }
}

int create_pidfile(int pid)
{
    FILE *fp = fopen(MASTER_PID_FILE, "w+");
    if (!fp)
        return -1;

    fprintf(fp, "%d", pid);
    fclose(fp);

    return 0;
}

int read_pidfile()
{
    char buff[32];
    FILE *fp = fopen(MASTER_PID_FILE, "r");
    if (!fp)
        return -1;

    if (NULL == fgets(buff, sizeof(buff) - 1, fp))
    {
        fclose(fp);
        return -2;
    }

    fclose(fp);
    return atoi(buff);
}

void delete_pidfile()
{
    struct stat statfile;

    if (stat(MASTER_PID_FILE, &statfile) == 0)
        remove(MASTER_PID_FILE);
}

