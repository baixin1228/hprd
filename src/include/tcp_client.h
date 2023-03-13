#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

int client_connect(uint32_t ip, uint16_t port);
int client_send_pkt(int fd, char *buf, size_t len);

#endif