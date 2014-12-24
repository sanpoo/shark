/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __SIGNAL_H__
#define __SIGNAL_H__

#include <signal.h>

#define SHUTDOWN_SIGNAL         SIGQUIT
#define TERMINATE_SIGNAL        SIGINT
#define RECONFIGURE_SIGNAL      SIGHUP
#define REOPEN_LOG_SIGNAL       SIGUSR1
#define CHANGEBIN_SIGNAL        SIGUSR2

void sys_signal_init();

#endif

