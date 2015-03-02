/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
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
    printf("Usage: shark [-?hvt] [-s signal]                                \n\n"
           "Options:                                                        \n"
           "  -?,-h     : this help                                         \n"
           "  -v        : show version and exit                             \n"
           "  -t        : check current config file and print conf, then exit\n"
           "  -s signal : send signal to a master process: stop, exit\n"
           "              stop :stop accepting new connections, and wait for established connections to be handled over\n"
           "              exit :direct exit, do NOT wait established connections to be handled over\n");
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
        printf("shark runs under linux on x86 or x86_64 platform\n"
               "shark version :"SHARK_VER"\n"
               "bug report to :sanpoos@gmail.com\n");
        exit(0);
    }

    if (argc == 2 && str_equal(argv[1], "-t"))
    {
        load_conf(CONF_FILE);
        conf_env_init();
        printf("configuration file %s test successful\n", CONF_FILE);
        print_conf();
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
    g_master_pid = getpid();
    proc_title_init(argv);
    sys_res_init();
    sys_signal_init();

    load_conf(CONF_FILE);
    conf_env_init();
    shm_init();
    log_init();

    tcp_srv_init();
    print_conf();
    print_runtime_var();
    create_pidfile(g_master_pid);
    process_init();
    master_process_cycle();
    worker_process_cycle();

    return 0;
}

