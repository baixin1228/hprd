
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "util.h"
#include "protocol.h"
#include "net_help.h"
#include "input_event.h"

#define MAX_EVENTS 1024
#define BUFLEN 1024 * 1024 * 10

#define ONE_MINUTE 60 * 1000

#define ONLISTEN 1
#define NOT_ACTIVE 0

char video_data[BUFLEN];

static inline void perr(int status, const char *func_info) {
	if (status) {
		perror(func_info);
		exit(EXIT_FAILURE);
	}
}

struct ep_event {
	int fd;     //cfd listenfd
	struct in_addr addr;
	uint16_t port;
	int events; //EPOLLIN  EPLLOUT
	void (*callback_fn)(int epfd, void *ev);
	int status;
	char recv_buf[1024 * 1024];
	char *send_buf;
	uint32_t send_len;
	uint32_t bradcast_idx;
	time_t last_active;
	struct ep_event *next;
};

void callback_recvdata(int epfd, struct ep_event *ev);
void callback_accept(int epfd, struct ep_event *ev);
void callback_senddata(int epfd, struct ep_event *ev);

static struct ep_event *client_event_head = NULL;
static int client_events_len = 0;
static uint32_t bradcast_idx = 0;
static pthread_spinlock_t g_lock;

/* create and add to my_events */
struct ep_event *create_ep_event(int fd, int events, void *callback_fn) {
	struct ep_event *ev = calloc(1, sizeof(struct ep_event));

	ev->fd = fd;
	ev->events = events;
	ev->callback_fn = (void (*)(int, void *))callback_fn;
	ev->status = NOT_ACTIVE;
	ev->send_buf = NULL;
	ev->send_len = 0;
	ev->bradcast_idx = bradcast_idx;
	ev->last_active = time(0);
	ev->next = NULL;

	return ev;
}

void register_event(int epfd, struct ep_event *ev) {
	int op = 0;
	if (ev->status == ONLISTEN) {
		op = EPOLL_CTL_MOD;
	} else {
		ev->status = ONLISTEN;
		op = EPOLL_CTL_ADD;
	}
	struct epoll_event event = {
		.events = ev->events,
		.data.ptr = ev
	};
	epoll_ctl(epfd, op, ev->fd, &event);
}

struct ep_event *new_client_ep_event(int epfd,
									int new_fd,
									int events,
									void *callback_fn) {

	struct ep_event *ev;
	struct ep_event *tmp;

	if (client_events_len == MAX_EVENTS) {
		log_error("Max Events Limit\n");
		return NULL;
	}

	ev = create_ep_event(new_fd, events, (void *)callback_recvdata);
	register_event(epfd, ev);

	if(client_event_head == NULL)
		client_event_head = ev;
	else {
		tmp = client_event_head;
		while(tmp->next != NULL)
			tmp = tmp->next;

		tmp->next = ev;
	}
	client_events_len++;
	return ev;
}

int remove_event(int epfd, struct ep_event *ev) {
	struct ep_event *tmp1 = NULL;
	struct ep_event *tmp2 = NULL;

	struct epoll_event event = {
		.events = ev->events,
		.data.ptr = ev
	};
	int err = epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &event);
	if(err == -1) {
		log_error("epoll_ctl EPOLL_CTL_DEL fail.");
		exit(-1);
	}
	log_info("remove event.");

	close(ev->fd);
	client_events_len--;
	ev->status = NOT_ACTIVE;

	tmp1 = client_event_head;
	while(tmp1 != NULL) {
		if(tmp1 == ev) {
			if(tmp1 == client_event_head) {
				client_event_head = NULL;
			} else {
				tmp2->next = tmp1->next;
				tmp1->next = NULL;
			}
			log_info("[删除链接] addr:%s:%d\n", inet_ntoa(ev->addr),
					 ntohs(ev->port));
			free(tmp1);
			return 0;
		}
		tmp2 = tmp1;
		tmp1 = tmp1->next;
	}
	return -1;
}

void callback_accept(int epfd, struct ep_event *ev) {
	struct sockaddr_in addr;

	pthread_spin_lock(&g_lock);
	socklen_t len = sizeof(addr);
	int new_fd = accept(ev->fd, (struct sockaddr *)&addr, &len);
	perr(new_fd == -1, __func__);
	fcntl(new_fd, F_SETFL, O_NONBLOCK);
	ev = new_client_ep_event(epfd, new_fd, EPOLLIN, (void *)callback_recvdata);
	ev->addr = addr.sin_addr;
	ev->port = addr.sin_port;
	if(ev == NULL) {
		log_info("[建立错误] addr:%s:%d\n", inet_ntoa(addr.sin_addr),
				 ntohs(addr.sin_port));
	} else {
		log_info("[建立连接] addr:%s:%d\n", inet_ntoa(addr.sin_addr),
				 ntohs(addr.sin_port));
	}
	pthread_spin_unlock(&g_lock);
}

static void _on_server_pkt(char *buf, size_t len) {
	struct data_pkt *pkt = (struct data_pkt *)buf;
	switch(pkt->cmd) {
	case INPUT_EVENT: {
		log_info("input event\n");
		break;
	}
	}
}

void callback_recvdata(int epfd, struct ep_event *ev) {
	pthread_spin_lock(&g_lock);
	if(tcp_recv_pkt(ev->fd, ev->recv_buf, _on_server_pkt) == -1) {
		remove_event(epfd, ev);
	}

	ev->last_active = time(0);

	// ev->callback_fn = (void (*)(int, void *))callback_senddata;
	// ev->events = EPOLLOUT;
	// register_event(epfd, ev);
	pthread_spin_unlock(&g_lock);
}

void callback_senddata(int epfd, struct ep_event *ev) {
	pthread_spin_lock(&g_lock);
	if(ev->send_buf == NULL || ev->send_len == 0)
		goto EXIT2;

	if(tcp_send_pkt(ev->fd, ev->send_buf, ev->send_len) == -1) {
		remove_event(epfd, ev);
		goto EXIT;
	}
	log_info("send data.");

	if(ev->bradcast_idx < bradcast_idx)
		ev->bradcast_idx++;

	ev->send_buf = NULL;
	ev->send_len = 0;

EXIT2:
	ev->callback_fn = (void (*)(int, void *))callback_recvdata;
	ev->events = EPOLLIN;
	register_event(epfd, ev);

EXIT:
	pthread_spin_unlock(&g_lock);
}

int create_ls_socket(char *ip, uint16_t port) {
	int ls_fd = socket(AF_INET, SOCK_STREAM, 0);
	fcntl(ls_fd, F_SETFL, O_NONBLOCK);
	struct sockaddr_in ls_addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = inet_addr(ip),
		.sin_port = htons(port)
	};
	bind(ls_fd, (struct sockaddr *)&ls_addr, sizeof(ls_addr));
	listen(ls_fd, 1024);
	return ls_fd;
}

void check_active(int epfd) {
	struct ep_event *tmp;
	time_t now = time(0);

	pthread_spin_lock(&g_lock);
	tmp = client_event_head;
	while(tmp != NULL) {
		time_t duration = now - tmp->last_active;
		if (duration >= 60) {
			remove_event(epfd, tmp);
		}
		tmp = tmp->next;
	}
	pthread_spin_unlock(&g_lock);
}

static struct server_info {
	int epfd;
	char ip[64];
	uint16_t port;
} ser_info;
static struct ep_event *accept_ep_event;

void *tcp_server_thread(void *opaque) {
	int ls_fd = create_ls_socket(ser_info.ip, ser_info.port);
	accept_ep_event = create_ep_event(ls_fd, EPOLLIN,
									  (void *)callback_accept);

	register_event(ser_info.epfd, accept_ep_event);

	struct epoll_event events[MAX_EVENTS + 1];
	while (1) {
		check_active(ser_info.epfd);
		int nfd = epoll_wait(ser_info.epfd, events, MAX_EVENTS + 1, ONE_MINUTE);
		perr(nfd <= 0, "epoll_wait");
		for (int i = 0; i < nfd; i++) {
			struct ep_event *ev = events[i].data.ptr;
			ev->callback_fn(ser_info.epfd, ev);
		}
	}
}

int tcp_server_init(char *ip, uint16_t port) {
	pthread_t p1;
	int epfd = epoll_create(MAX_EVENTS + 1);
	ser_info.epfd = epfd;
	strcpy(ser_info.ip, ip);
	ser_info.port = port;

	pthread_spin_init(&g_lock, 0);
	pthread_create(&p1, NULL, tcp_server_thread, NULL);
	pthread_detach(p1);
	return epfd;
}

void server_bradcast_data(int epfd, char *buf, uint32_t len) {
	struct ep_event *tmp;

	pthread_spin_lock(&g_lock);
	log_info("bradcast data.");

	tmp = client_event_head;
	while(tmp != NULL) {
		while(tmp->bradcast_idx < bradcast_idx)
			usleep(5000);

		tmp = tmp->next;
	}

	memcpy(video_data, buf, len);
	bradcast_idx++;

	tmp = client_event_head;
	while(tmp != NULL) {
		tmp->callback_fn = (void (*)(int, void *))callback_senddata;
		tmp->send_buf = video_data;
		tmp->send_len = len;
		tmp->events = EPOLLOUT;
		register_event(epfd, tmp);

		tmp = tmp->next;
	}
	pthread_spin_unlock(&g_lock);
}

int get_client_count(void)
{
	return client_events_len;
}