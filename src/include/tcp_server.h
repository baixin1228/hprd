#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

void *tcp_server_thread(void *opaque);
void bradcast_data2(char *buf, uint32_t len);

#endif