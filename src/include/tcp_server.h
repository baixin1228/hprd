#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

int tcp_server_init(char *ip, uint16_t port);
void server_bradcast_data(int epfd, char *buf, uint32_t len);
int get_client_count(void);

#endif