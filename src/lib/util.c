/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "pub.h"
#include "env.h"
#include "util.h"

static char **shark_argv;
extern char **environ;

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

void sys_rlimit_init()
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

void set_proc_title(const char *name)
{
    strcpy(shark_argv[0], name);
}

void proc_title_init(char **argv)
{
    int i;
    size_t len = 0;
    void *p;

    shark_argv = argv;

    for (i = 1; argv[i]; i++)
        len += strlen(argv[i]) + 1;

    for (i = 0; environ[i]; i++)
        len += strlen(environ[i]) + 1;

    p = malloc(len);
    if (!p)
    {
        printf("Failed to alloc environ mem\n");
        exit(0);
    }

    memcpy(p, argv[1] ? argv[1] : environ[0], len);

    len = 0;
    for (i = 1; argv[i]; i++)
    {
        argv[i] = p + len;
        len += strlen(argv[i]) + 1;
    }

    for (i = 0; environ[i]; i++)
    {
        environ[i] = p + len;
        len += strlen(environ[i]) + 1;
    }
}

void create_pidfile(int pid)
{
    FILE *fp = fopen(MASTER_PID_FILE, "w+");
    if (!fp)
    {
        printf("%s %s\n", strerror(errno), MASTER_PID_FILE);
        exit(0);
    }

    fprintf(fp, "%d", pid);
    fclose(fp);
}

int read_pidfile()
{
    char buff[32];
    FILE *fp = fopen(MASTER_PID_FILE, "r");
    if (!fp)
    {
        printf("%s -> %s\n", strerror(errno), MASTER_PID_FILE);
        exit(0);
    }

    if (NULL == fgets(buff, sizeof(buff) - 1, fp))
    {
        fclose(fp);
        exit(0);
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

struct tm *get_tm()
{
    time_t t;

    time(&t);
    return localtime(&t);
}

long long get_curr_mseconds()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

