#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__
#include <glib.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "queue.h"

#define MAX_EVENTS 1024

struct tcp_server_client {
	int fd;     //cfd listenfd
	uint32_t nip;
	uint16_t nport;
	uint32_t client_id;
	int events; //EPOLLIN  EPLLOUT
	struct data_queue recv_queue;
	struct data_queue send_queue;
	pthread_mutex_t send_buf_lock;
	pthread_cond_t send_buf_cond;
	time_t last_active;
	void *priv;
};

int tcp_server_init(char *ip, uint16_t port);
int get_client_count(void);
int tcp_send_data_safe(struct tcp_server_client *ev, char *buf, uint32_t len);
int tcp_send_data_unsafe(struct tcp_server_client *ev, char *buf, uint32_t len);
void tcp_foreach_client(void (*callback)(gpointer key, gpointer value, gpointer user_data), void *opaque);
struct tcp_server_client *tcp_find_client(uint32_t client_id);

#endif