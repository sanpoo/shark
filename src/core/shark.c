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
#include "strop.h"
#include "env.h"
#include "util.h"
#include "shm.h"
#include "sys_signal.h"
#include "process.h"

static void useage(int argc, char *argv[])
{
    printf("Usage: shark [-?hvt] [-s signal]                            \n\n"
           "Options:                                                      \n"
           "   -?,-h         : this help                                  \n"
           "   -v            : show version and exit                      \n"
           "   -t            : print running shark config and exit        \n"
           "   -s signal     : send signal to a master process: stop, quit, reopen, reload\n"
           "                   stop: stop accept new connection and wait all conneciont to be handled\n"
           "                   quit: direct exit, do not wait all connection handled\n"
           "                   reopen: reopen log file\n"
           "                   reload: reboot shark with all connection handled\n");
}

static void send_signal_to_master(int signo)
{
    int pid = read_pidfile();
    if (pid <= 0)
        printf("shark maybe not running or pid file removed.\n");
    else
        kill(pid, signo);
}

static void handle_args(int argc, char *argv[])
{
    if (argc == 1)
        return;

    if (str_equal(argv[1], "-v"))
    {
        printf("shark runs under linux on x86 or x86_64 platform\n"
               "shark version: "SHARK_VER"\n"
               "Bug report to :sanpoos@gmail.com\n");
        exit(0);
    }

    if (str_equal(argv[1], "-t"))
    {
        printf("to be implement...\n");
        exit(0);
    }

    if (argc == 3 && str_equal(argv[1], "-s") && str_equal(argv[2], "stop"))
    {
        send_signal_to_master(SHUTDOWN_SIGNAL);
        exit(0);
    }

    if (argc == 3 && str_equal(argv[1], "-s") && str_equal(argv[2], "quit"))
    {
        send_signal_to_master(TERMINATE_SIGNAL);
        exit(0);
    }

    if (argc == 3 && str_equal(argv[1], "-s") && str_equal(argv[2], "reopen"))
    {
        printf("to be implement...\n");
        exit(0);
    }

    if (argc == 3 && str_equal(argv[1], "-s") && str_equal(argv[2], "reload"))
    {
        printf("to be implement...\n");
        exit(0);
    }

    useage(argc, argv);
    exit(0);
}

int main(int argc, char **argv)
{
    handle_args(argc, argv);

    sys_daemon();
    sys_env_var_init();
    sys_signal_init();
    sys_res_init();

    load_conf("../conf/shark.conf");
    conf_env_init();
    shm_init();
    log_init();

    tcp_srv_init();
    print_sys_env_var();

    if (create_pidfile(g_master_pid))
    {
        printf("Failed to write master pid to file\n");
        exit(0);
    }

    process_init();
    if (log_worker_alloc(g_process_id) < 0)
    {
        printf("Failed to alloc log for process:%d\n", g_process_id);
        exit(0);
    }

//worker_process_cycle();
//return 0;

    if (g_process_id == g_master_pid)
        master_process_cycle();
    else
        worker_process_cycle();

    return 0;
}

