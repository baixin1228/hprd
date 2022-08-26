#ifndef __SERVER_SOCKET_H__
#define __SERVER_SOCKET_H__
#include <stdint.h>
#include <stdbool.h>

#include "server_net.h"

int udp_send_pkt(struct ser_net_obj *obj, uint8_t *buf, size_t len);
int tcp_send_pkt(struct ser_net_obj *obj, uint8_t *buf, size_t len);
#endif