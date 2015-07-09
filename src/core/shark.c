/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "conf.h"
#include "log.h"
#include "str.h"
#include "env.h"
#include "util.h"
#include "shm.h"
#include "sys_signal.h"
#include "process.h"

static void useage(int argc, char *argv[])
{
    printf("Usage: shark [-?hvct] [-s signal]     "LINEFEED LINEFEED
           "Options:                              "LINEFEED
           "  -?,-h     : this help               "LINEFEED
           "  -v        : show version            "LINEFEED
           "  -c        : show config file content"LINEFEED
           "  -t        : check and show config file content"LINEFEED
           "  -s signal : send signal to a master process: stop, exit"LINEFEED
           "              stop : stop accepting, and wait for connections to be handled over"LINEFEED
           "              exit : direct exit"LINEFEED);
}

static void send_signal_to_master(int signo)
{
    int pid = read_pidfile();
    kill(pid, signo);
}

static void handle_args(int argc, char *argv[])
{
    if (argc == 1)
        return;

    if (argc == 2 && str_equal(argv[1], "-v"))
    {
        printf("shark runs under linux on x86 or x86_64 platform"LINEFEED
               "shark version : "SHARK_VER LINEFEED
               "bug report to : sanpoos@gmail.com"LINEFEED);
        exit(0);
    }

    if (argc == 2 && str_equal(argv[1], "-c"))
    {
        load_raw_conf(CONF_FILE);
        print_raw_conf();
        exit(0);
    }

    if (argc == 2 && str_equal(argv[1], "-t"))
    {
        load_raw_conf(CONF_FILE);
        conf_env_init();
        print_env();
        exit(0);
    }

    if (argc == 3 && str_equal(argv[1], "-s") && str_equal(argv[2], "stop"))
    {
        send_signal_to_master(SHUTDOWN_SIGNAL);
        exit(0);
    }

    if (argc == 3 && str_equal(argv[1], "-s") && str_equal(argv[2], "exit"))
    {
        send_signal_to_master(TERMINATE_SIGNAL);
        exit(0);
    }

    useage(argc, argv);
    exit(0);
}

int main(int argc, char **argv)
{
    sys_env_init();
    handle_args(argc, argv);

    sys_daemon();
    sys_rlimit_init();
    sys_signal_init();
    proc_title_init(argv);

    load_raw_conf(CONF_FILE);
    conf_env_init();
    shm_init();
    log_init();

    tcp_srv_init();
    process_init();
    master_process_cycle();
    worker_process_cycle();

    return 0;
}

