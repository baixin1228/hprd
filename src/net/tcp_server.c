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
#include "net/net_util.h"
#include "net/tcp_server.h"
#include "net/net_server.h"

struct {
	int tcp_fd;
	int epoll_fd;
	uint32_t nip;
	uint16_t nport;
	pthread_spinlock_t lock;
	GHashTable *client_table;
} tcp_server = {0};

/* create and add to my_events */
static struct tcp_server_client *_create_tcp_server_client(int fd, int events) {
	struct tcp_server_client *ev = calloc(1, sizeof(struct tcp_server_client));

	ev->fd = fd;
	ev->events = events;
	ev->last_active = time(0);
	pthread_mutex_init(&ev->send_buf_lock, NULL);
	pthread_cond_init(&ev->send_buf_cond, NULL);
	return ev;
}

static void _add_event(int epfd, struct tcp_server_client *ev) {
	int err;
	struct epoll_event event = {
		.events = ev->events,
		.data.ptr = ev
	};
	err = epoll_ctl(epfd, EPOLL_CTL_ADD, ev->fd, &event);
	if(err == -1) {
		log_error("epoll_ctl EPOLL_CTL_ADD fail, fd:%d", ev->fd);
		exit(-1);
	}
}

static void _mod_event(int epfd, struct tcp_server_client *ev) {
	int err;
	struct epoll_event event = {
		.events = ev->events,
		.data.ptr = ev
	};
	err = epoll_ctl(epfd, EPOLL_CTL_MOD, ev->fd, &event);
	if(err == -1) {
		log_error("epoll_ctl EPOLL_CTL_MOD fail, fd:%d", ev->fd);
		exit(-1);
	}
}

__attribute__((unused))
static void _remove_event(int epfd, struct tcp_server_client *ev) {
	int err;
	struct epoll_event event = {
		.events = ev->events,
		.data.ptr = ev
	};
	err = epoll_ctl(epfd, EPOLL_CTL_DEL, ev->fd, &event);
	if(err == -1) {
		log_error("epoll_ctl EPOLL_CTL_DEL fail, fd:%d", ev->fd);
		exit(-1);
	}
}

static int _remove_client(int epfd, struct tcp_server_client *ev) {
	int ret = -1;

	log_info("remove event, fd:%d client_id:%x", ev->fd, ev->client_id);
	close(ev->fd);

	if(g_hash_table_contains(tcp_server.client_table, &ev->client_id))
	{
		g_hash_table_remove(tcp_server.client_table, &ev->client_id);
		struct in_addr addr;
		addr.s_addr = ev->nip;
		log_info("[删除链接] addr:%s:%d\n", inet_ntoa(addr), ntohs(ev->nport));
		free(ev);
		ret = 0;
	}else{
		log_warning("remove client fail, not find client client_id:%x", ev->client_id);
	}

	return ret;
}

static void _callback_accept(int epfd, struct tcp_server_client *ser_obj) {
	struct sockaddr_in addr;
	struct tcp_server_client *ev;

	if (g_hash_table_size(tcp_server.client_table) == MAX_EVENTS) {
		log_error("Max Events Limit\n");
		return;
	}

	socklen_t len = sizeof(addr);
	int new_fd = accept(ser_obj->fd, (struct sockaddr *)&addr, &len);
	if(new_fd == -1)
	{
		log_perr("%s", __func__);
		exit(EXIT_FAILURE);
	}
	fcntl(new_fd, F_SETFL, O_NONBLOCK);

	ev = _create_tcp_server_client(new_fd, EPOLLIN);
	if(ev == NULL) {
		log_info("[建立错误] addr:%s:%d\n", inet_ntoa(addr.sin_addr),
				 ntohs(addr.sin_port));
	} else {
		ev->nip = addr.sin_addr.s_addr;
		ev->nport = addr.sin_port;
		_add_event(epfd, ev);
		ev->client_id = server_client_newid();

		pthread_spin_lock(&tcp_server.lock);
		g_hash_table_insert(tcp_server.client_table, &ev->client_id, ev);
		pthread_spin_unlock(&tcp_server.lock);
		tcp_server_client_connect(ev);
		log_info("[建立连接] addr:%s:%d new_fd:%d client_id:%x\n",
			inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), new_fd,
			ev->client_id);
	}
}

static int _check_recv_pkt(struct tcp_server_client *ev)
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
				server_on_pkg((struct server_client *)ev->priv, buf_ptr + 4,
					pkt_len);
				queue_tail_point_forward(&ev->recv_queue, 4 + pkt_len);
				return 0;
			}
		}else{
			log_error("recv len is invalid.");
		}
	}
	return -1;
}

static void _callback_recvdata(int epfd, struct tcp_server_client *ev) {
	int ret;
	char *buf_ptr;
	buf_ptr = queue_get_write_ptr(&ev->recv_queue);
	if((ret = recv(ev->fd, buf_ptr, queue_get_end_free_count(&ev->recv_queue), 0)) == 0) {
		log_error("recv error.");
		pthread_spin_lock(&tcp_server.lock);
		_remove_client(epfd, ev);
		pthread_spin_unlock(&tcp_server.lock);
		return;
	}
	if(ret > 0)
		queue_head_point_forward(&ev->recv_queue, ret);

	while(_check_recv_pkt(ev) == 0);
	ev->last_active = time(0);
}

static void _callback_senddata(int epfd, struct tcp_server_client *ev) {
	int ret;
	char *buf_ptr;
	static int time = 0;
	pthread_mutex_lock(&ev->send_buf_lock);
	buf_ptr = queue_get_read_ptr(&ev->send_queue);

	if((ret = send(ev->fd, buf_ptr, queue_get_end_data_count(&ev->send_queue), 0)) == 0) {
		log_error("send error.");
		pthread_spin_lock(&tcp_server.lock);
		_remove_client(epfd, ev);
		pthread_spin_unlock(&tcp_server.lock);
		goto EXIT;
	}

	if(ret > 0)
		queue_tail_point_forward(&ev->send_queue, ret);

	pthread_mutex_unlock(&ev->send_buf_lock);
	if(ret > 0 && time < 10)
		pthread_cond_signal(&ev->send_buf_cond);

	time++;
	ev->events = EPOLLIN;
	_mod_event(epfd, ev);

EXIT:
	return;
}

static int _create_ls_socket(uint32_t nip, uint16_t nport) {
	int ret;
	int ls_fd = socket(AF_INET, SOCK_STREAM, 0);
	fcntl(ls_fd, F_SETFL, O_NONBLOCK);
	struct sockaddr_in ls_addr = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = nip,
		.sin_port = nport
	};
	ret = bind(ls_fd, (struct sockaddr *)&ls_addr, sizeof(ls_addr));
	if(ret == -1)
	{
		log_error("bind fail.");
		return -1;
	}
	ret = listen(ls_fd, 1024);
	if(ret == -1)
	{
		log_error("listen fail.");
		return -1;
	}
	return ls_fd;
}

struct tcp_server_client *tcp_find_client(uint32_t client_id)
{
	struct tcp_server_client *ret = NULL;

	pthread_spin_lock(&tcp_server.lock);
	if(g_hash_table_contains(tcp_server.client_table, &client_id))
	{
		ret = (struct tcp_server_client *)g_hash_table_lookup(tcp_server.client_table, &client_id);
	}
	pthread_spin_unlock(&tcp_server.lock);
	return ret;
}

int get_client_count(void)
{
	int ret;
	pthread_spin_lock(&tcp_server.lock);
	ret = g_hash_table_size(tcp_server.client_table);
	pthread_spin_unlock(&tcp_server.lock);
	return ret;
}

int tcp_send_data_safe(struct tcp_server_client *ev, char *buf, uint32_t len)
{
	int ret = -1;
	uint32_t net_len = htonl(len);

	pthread_mutex_lock(&ev->send_buf_lock);
	do{
		ret = enqueue_data(&ev->send_queue, (uint8_t *)&net_len, 4);
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
		ret = enqueue_data(&ev->send_queue, (uint8_t *)buf, len);
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

int tcp_send_data_unsafe(struct tcp_server_client *ev, char *buf, uint32_t len)
{
	if(queue_get_free_count(&ev->send_queue) < len + 4)
	{
		log_warning("send slow, w:%d e:%d skip.", len,
			queue_get_free_count(&ev->send_queue));
		return -1;
	}

	return tcp_send_data_safe(ev, buf, len);
}

void tcp_foreach_client(void (*callback)(gpointer key, gpointer value, gpointer user_data), void *opaque) {
	pthread_spin_lock(&tcp_server.lock);
	g_hash_table_foreach(tcp_server.client_table, callback, opaque);
	pthread_spin_unlock(&tcp_server.lock);
}

static int _foreach_active(gpointer key, gpointer value, gpointer user_data)
{
	time_t duration;
	time_t now = time(0);
	int *epfd = (int *)user_data;
	struct tcp_server_client *ev = (struct tcp_server_client *)value;

	duration = now - ev->last_active;
	if (duration >= 5) {
		log_warning("client time out, clean.");
		_remove_client(*epfd, ev);
		return true;
	}
	return false;
}

__attribute__((unused))
static void _check_active() {
	pthread_spin_lock(&tcp_server.lock);
	g_hash_table_foreach_remove(tcp_server.client_table, _foreach_active,
		&tcp_server.epoll_fd);
	pthread_spin_unlock(&tcp_server.lock);
}

static void _foreach_send(gpointer key, gpointer value, gpointer user_data)
{
	int *epfd = (int *)user_data;
	struct tcp_server_client *ev = (struct tcp_server_client *)value;

	if(get_queue_data_count(&ev->send_queue) > 0) {
		ev->events = EPOLLOUT;
		_mod_event(*epfd, ev);
	}
}

static void _check_need_send() {
	pthread_spin_lock(&tcp_server.lock);
	g_hash_table_foreach(tcp_server.client_table, _foreach_send,
		&tcp_server.epoll_fd);
	pthread_spin_unlock(&tcp_server.lock);
}

static void _process_event(struct epoll_event *event)
{
	struct tcp_server_client *ev = event->data.ptr;

	switch(event->events)
	{
		case EPOLLIN:
		{
			if(ev->fd == tcp_server.tcp_fd)
			{
				_callback_accept(tcp_server.epoll_fd, ev);
			}else{
				_callback_recvdata(tcp_server.epoll_fd, ev);
			}
			break;
		}
		case EPOLLOUT:
		{
			if(ev->fd != tcp_server.tcp_fd)
			{
				_callback_senddata(tcp_server.epoll_fd, ev);
			}else{
				log_warning("EPLLOUT event, fd:%d", ev->fd);
			}
			break;
		}
		case EPOLLERR:
		case EPOLLHUP:
		{
			pthread_spin_lock(&tcp_server.lock);
			_remove_client(tcp_server.epoll_fd, ev);
			pthread_spin_unlock(&tcp_server.lock);
			log_warning("EPOLLHUP event, remove client.");
			break;
		}
		default:
		{
			pthread_spin_lock(&tcp_server.lock);
			_remove_client(tcp_server.epoll_fd, ev);
			pthread_spin_unlock(&tcp_server.lock);
			log_warning("unknow EPOLL event:%d", event->events);
			break;
		}
	}
}

static void *_tcp_server_thread(void *opaque) {
	int nfd;
	struct tcp_server_client *accept_tcp_client;

	tcp_server.tcp_fd = _create_ls_socket(tcp_server.nip, tcp_server.nport);
	if(tcp_server.tcp_fd == -1)
		exit(-1);

	accept_tcp_client = _create_tcp_server_client(tcp_server.tcp_fd, EPOLLIN);
	_add_event(tcp_server.epoll_fd, accept_tcp_client);

	struct epoll_event events[MAX_EVENTS + 1];
	while (1) {
		// _check_active();
		nfd = epoll_wait(tcp_server.epoll_fd, events, MAX_EVENTS + 1, 10);
		if(nfd < 0)
		{
			log_perr("epoll_wait");
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < nfd; i++) {
			_process_event(&events[i]);
		}
		_check_need_send();
	}
}

int tcp_server_init(char *ip, uint16_t port) {
	pthread_t p1;

	log_info("tcp server init.");
	pthread_spin_init(&tcp_server.lock, PTHREAD_PROCESS_SHARED);

	tcp_server.client_table = g_hash_table_new(g_int_hash, g_int_equal);
	tcp_server.epoll_fd = epoll_create(MAX_EVENTS + 1);
	tcp_server.nip = inet_addr(ip);
	tcp_server.nport = ntohs(port);

	pthread_create(&p1, NULL, _tcp_server_thread, NULL);
	pthread_detach(p1);
	return 0;
}