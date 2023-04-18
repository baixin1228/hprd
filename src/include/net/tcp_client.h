#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

int tcp_client_init(char *ip, uint16_t port);
int tcp_client_send_data(char *buf, size_t len);
void tcp_client_release();

#endif