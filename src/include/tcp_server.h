#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__
#include <pthread.h>
#include <arpa/inet.h>

#include "queue.h"

#define MAX_EVENTS 1024

struct ep_event {
	int fd;     //cfd listenfd
	struct in_addr addr;
	uint16_t port;
	int events; //EPOLLIN  EPLLOUT
	struct data_queue recv_queue;
	struct data_queue send_queue;
	pthread_mutex_t send_buf_lock;
	pthread_cond_t send_buf_cond;
	time_t last_active;
	struct ep_event *next;
};

int tcp_server_init(char *ip, uint16_t port);
void server_bradcast_data(char *buf, uint32_t len);
int get_client_count(void);
int server_send_data_safe(struct ep_event *ev, char *buf, uint32_t len);

#endif