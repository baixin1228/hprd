#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#include "util.h"
#include "protocol.h"
#include "net_help.h"
#include "tcp_server.h"
#include "protocol.h"

static int client_events_len = 0;
static struct ep_event *client_event_head = NULL;

/* create and add to my_events */
struct ep_event *create_ep_event(int fd, int events) {
	struct ep_event *ev = calloc(1, sizeof(struct ep_event));

	ev->fd = fd;
	ev->events = events;
	ev->last_active = time(0);
	ev->next = NULL;
	pthread_mutex_init(&ev->send_buf_lock, NULL);
	pthread_cond_init(&ev->send_buf_cond, NULL);
	return ev;
}

void add_event(int epfd, struct ep_event *ev) {
	int err;
	struct epoll_event event = {
		.events = ev->events,
		.data.ptr = ev
	};
	err = epoll_ctl(epfd, EPOLL_CTL_ADD, ev->fd, &event);
	if(err == -1) {
		log_error("epoll_ctl EPOLL_CTL_ADD fail.");
		exit(-1);
	}
}

void mod_event(int epfd, struct ep_event *ev) {
	int err;
	struct epoll_event event = {
		.events = ev->events,
		.data.ptr = ev
	};
	err = epoll_ctl(epfd, EPOLL_CTL_MOD, ev->fd, &event);
	if(err == -1) {
		log_error("epoll_ctl EPOLL_CTL_MOD fail.");
		exit(-1);
	}
}

void remove_event(int epfd, struct ep_event *ev) {
	int err;
	struct epoll_event event = {
		.events = ev->events,
		.data.ptr = ev
	};
	err = epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &event);
	if(err == -1) {
		log_error("epoll_ctl EPOLL_CTL_DEL fail.");
		exit(-1);
	}
}

struct ep_event *new_client_ep_event(int epfd,
									int new_fd,
									int events) {

	struct ep_event *ev;
	struct ep_event *tmp;

	if (client_events_len == MAX_EVENTS) {
		log_error("Max Events Limit\n");
		return NULL;
	}

	ev = create_ep_event(new_fd, events);
	add_event(epfd, ev);

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

int remove_client(int epfd, struct ep_event *ev) {
	struct ep_event *tmp1 = NULL;
	struct ep_event *tmp2 = NULL;


	log_info("remove event.");

	close(ev->fd);
	client_events_len--;

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

	socklen_t len = sizeof(addr);
	int new_fd = accept(ev->fd, (struct sockaddr *)&addr, &len);
	if(new_fd == -1)
	{
		log_perr("%s", __func__);
		exit(EXIT_FAILURE);
	}
	fcntl(new_fd, F_SETFL, O_NONBLOCK);
	ev = new_client_ep_event(epfd, new_fd, EPOLLIN);
	ev->addr = addr.sin_addr;
	ev->port = addr.sin_port;

	if(ev == NULL) {
		log_info("[建立错误] addr:%s:%d\n", inet_ntoa(addr.sin_addr),
				 ntohs(addr.sin_port));
	} else {
		log_info("[建立连接] addr:%s:%d new_fd:%d\n", inet_ntoa(addr.sin_addr),
				 ntohs(addr.sin_port), new_fd);
	}
}

void on_server_pkt(struct ep_event *ev, char *buf, size_t len);
int _check_recv_pkt(struct ep_event *ev)
{
	int pkt_len;
	char *buf_ptr;
	buf_ptr = queue_get_read_ptr(&ev->recv_queue);
	if(get_queue_data_count(&ev->recv_queue) > 4)
	{
		pkt_len = ntohl(*(uint32_t *)(buf_ptr));
		if(pkt_len > 0)
		{
			if(get_queue_data_count(&ev->recv_queue) >= 4 + pkt_len)
			{
				on_server_pkt(ev, buf_ptr + 4, pkt_len);
				queue_tail_point_forward(&ev->recv_queue, 4 + pkt_len);
				return 0;
			}
		}else{
			log_error("recv len is invalid.");
		}
	}
	return -1;
}

void callback_recvdata(int epfd, struct ep_event *ev) {
	int ret;
	char *buf_ptr;
	buf_ptr = queue_get_write_ptr(&ev->recv_queue);
	if((ret = recv(ev->fd, buf_ptr, queue_get_end_free_count(&ev->recv_queue), 0)) == 0) {
		log_error("recv error.");
		remove_client(epfd, ev);
		return;
	}
	if(ret > 0)
		queue_head_point_forward(&ev->recv_queue, ret);

	while(_check_recv_pkt(ev) == 0);
	ev->last_active = time(0);
}

void callback_senddata(int epfd, struct ep_event *ev) {
	int ret;
	char *buf_ptr;
	static int time = 0;
	pthread_mutex_lock(&ev->send_buf_lock);
	buf_ptr = queue_get_read_ptr(&ev->send_queue);

	if((ret = send(ev->fd, buf_ptr, queue_get_end_data_count(&ev->send_queue), 0)) == 0) {
		log_error("send error.");
		remove_client(epfd, ev);
		goto EXIT;
	}

	if(ret > 0)
		queue_tail_point_forward(&ev->send_queue, ret);

	pthread_mutex_unlock(&ev->send_buf_lock);
	if(ret > 0 && time < 10)
		pthread_cond_signal(&ev->send_buf_cond);

	time++;
	ev->events = EPOLLIN;
	mod_event(epfd, ev);

EXIT:
	return;
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
	time_t duration;

	tmp = client_event_head;
	while(tmp != NULL) {
		duration = now - tmp->last_active;
		// printf("check_active:%ld\n", duration);
		if (duration >= 5) {
			remove_client(epfd, tmp);
		}
		tmp = tmp->next;
	}
}

void check_need_send(int epfd) {
	struct ep_event *tmp;

	tmp = client_event_head;
	while(tmp != NULL) {
		if(get_queue_data_count(&tmp->send_queue) > 0) {
			tmp->events = EPOLLOUT;
			mod_event(epfd, tmp);
		}
		tmp = tmp->next;
	}

}

static struct server_info {
	int epfd;
	char ip[64];
	uint16_t port;
} ser_info;
static struct ep_event *accept_ep_event;

int server_fd = -1;
void process_event(struct epoll_event *event)
{
	struct ep_event *ev = event->data.ptr;

	switch(event->events)
	{
		case EPOLLIN:
		{
			if(ev->fd == server_fd)
			{
				callback_accept(ser_info.epfd, ev);
			}else{
				callback_recvdata(ser_info.epfd, ev);
			}
			break;
		}
		case EPOLLOUT:
		{
			if(ev->fd != server_fd)
			{
				callback_senddata(ser_info.epfd, ev);
			}else{
				log_warning("unknow EPLLOUT event, fd:%d", ev->fd);
			}
			break;
		}
		case EPOLLERR:
		case EPOLLHUP:
		{
			remove_client(ser_info.epfd, ev);
			break;
		}
		default:
		{
			remove_client(ser_info.epfd, ev);
			log_warning("unknow EPOLL event:%d", event->events);
			break;
		}
	}
}

void *tcp_server_thread(void *opaque) {
	int nfd;
	server_fd = create_ls_socket(ser_info.ip, ser_info.port);
	accept_ep_event = create_ep_event(server_fd, EPOLLIN);

	add_event(ser_info.epfd, accept_ep_event);

	struct epoll_event events[MAX_EVENTS + 1];
	while (1) {
		// check_active(ser_info.epfd);
		nfd = epoll_wait(ser_info.epfd, events, MAX_EVENTS + 1, 10);
		if(nfd < 0)
		{
			log_perr("epoll_wait");
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < nfd; i++) {
			process_event(&events[i]);
		}
		check_need_send(ser_info.epfd);
	}
}

int tcp_server_init(char *ip, uint16_t port) {
	pthread_t p1;
	int epfd = epoll_create(MAX_EVENTS + 1);

	ser_info.epfd = epfd;
	strcpy(ser_info.ip, ip);
	ser_info.port = port;

	pthread_create(&p1, NULL, tcp_server_thread, NULL);
	pthread_detach(p1);
	return epfd;
}

int get_client_count(void)
{
	return client_events_len;
}

int server_send_data_safe(struct ep_event *ev, char *buf, uint32_t len)
{
	int ret = -1;
	uint32_t net_len = htonl(len);

	pthread_mutex_lock(&ev->send_buf_lock);
	do{
		ret = queue_append_data(&ev->send_queue, (uint8_t *)&net_len, 4);
		if(ret == 0)
		{
			log_warning("send slow, w:%d e:%d waiting.", 4,
				queue_get_free_count(&ev->send_queue));
			pthread_cond_wait(&ev->send_buf_cond, &ev->send_buf_lock);
		}
		if(ret == -1)
			return -1;
	}while(ret == 0);

	do{
		ret = queue_append_data(&ev->send_queue, (uint8_t *)buf, len);
		if(ret == 0)
		{
			log_warning("send slow, w:%d e:%d waiting.", len,
				queue_get_free_count(&ev->send_queue));
			pthread_cond_wait(&ev->send_buf_cond, &ev->send_buf_lock);
		}
		if(ret == -1)
			return -1;
	}while(ret == 0);

	pthread_mutex_unlock(&ev->send_buf_lock);
	return 0;
}

int server_send_data_unsafe(struct ep_event *ev, char *buf, uint32_t len)
{
	if(queue_get_free_count(&ev->send_queue) < len + 4)
	{
		log_warning("send slow, w:%d e:%d skip.", len,
			queue_get_free_count(&ev->send_queue));
		return -1;
	}

	return server_send_data_safe(ev, buf, len);
}

void server_bradcast_data(char *buf, uint32_t len) {
	struct ep_event *tmp;

	tmp = client_event_head;
	while(tmp != NULL) {
		server_send_data_unsafe(tmp, buf, len);
		tmp = tmp->next;
	}
}