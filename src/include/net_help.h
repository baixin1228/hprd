#ifndef __NET_HELP_H__
#define __NET_HELP_H__
#include "common.h"

int tcp_send_pkt(int fd, char *buf, size_t len);
int tcp_recv_pkt(int fd, char *_recv_buf, void (*callback)(char *buf, size_t len));

#endif