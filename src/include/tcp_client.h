#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

void *tcp_client_thread(void *opaque);
int client_send_pkt(char *buf, size_t len);

#endif