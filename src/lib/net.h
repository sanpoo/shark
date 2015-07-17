/*
    Copyright (C) 2014 bo.shen. All Rights Reserved.
    Author: bo.shen <sanpoos@gmail.com>
*/
#ifndef __NET_H__
#define __NET_H__

int set_nonblock(int fd);
int enable_tcp_no_delay(int fd);
int disable_tcp_no_delay(int fd);
int set_keep_alive(int fd, int interval);
int set_reuse_addr(int fd);

unsigned fd_to_nl(int fd);
char *get_peer_ip(int fd);
unsigned ip_to_nl(const char *ip);

#endif

