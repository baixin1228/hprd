// #if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
// #include <windows.h>
// #elif !defined(__unix)
// #define __unix
// #endif

// #ifdef __unix
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
// #endif

#include "util.h"
#include "queue.h"
#include "net/net_util.h"
#include "net/kcp_server.h"
#include "net/net_server.h"
#include "net/isleep.h"

struct {
	int sockfd;
	GHashTable *client_table;
	pthread_spinlock_t kcp_lock;
	pthread_spinlock_t table_lock;
} kcp_server = {0};

static int _udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	struct sockaddr_in send_addr;
	struct kcp_server_client *client = (struct kcp_server_client *)user;

	send_addr.sin_port = client->nport;
	send_addr.sin_addr.s_addr = client->nip;
	send_addr.sin_family = client->family;

	// log_info("udp send %s:%d len:%d\n", inet_ntoa(send_addr.sin_addr), ntohs(send_addr.sin_port), len);
	if(kcp_server.sockfd != -1)
		sendto(kcp_server.sockfd, buf, len, 0, (struct sockaddr *)&send_addr, sizeof(send_addr));

	return 0;
}

static struct kcp_server_client *_new_client(uint32_t nip, uint16_t nport, uint16_t family, uint32_t client_id)
{
	struct kcp_server_client *ret = calloc(sizeof(struct kcp_server_client), 1);
	if(ret == NULL)
	{
		log_error("[%s] calloc error.", __func__);
		exit(-1);
	}
	ret->nip = nip;
	ret->nport = nport;
	ret->family = family;

	// 创建两个端点的 kcp对象，第一个参数 conv是会话编号，同一个会话需要相同
	// 最后一个是 user参数，用来传递标识
	ret->kcp_context = ikcp_create(htonl(client_id), (void*)ret);
	struct in_addr addr;
	addr.s_addr = nip;
	log_info("ikcp_create, %s:%d client_id:%u", inet_ntoa(addr), ntohs(nport), client_id);
	// 设置kcp的下层输出，这里为 _udp_output，模拟udp网络输出函数
	ret->kcp_context->output = _udp_output;

	// 配置窗口大小：平均延迟200ms，每20ms发送一个包，
	// 而考虑到丢包重发，设置最大收发窗口为128
	ikcp_wndsize(ret->kcp_context, 128, 128);

	// 启动快速模式
	// 第二个参数 nodelay-启用以后若干常规加速将启动
	// 第三个参数 interval为内部处理时钟，默认设置为 10ms
	// 第四个参数 resend为快速重传指标，设置为2
	// 第五个参数 为是否禁用常规流控，这里禁止
	ikcp_nodelay(ret->kcp_context, 2, 10, 2, 1);
	ret->kcp_context->rx_minrto = 10;
	ret->kcp_context->fastresend = 1;

	return ret;
}

static inline int _kcp_send_all(ikcpcb * kcp, char *buf, size_t len)
{
	int ret;
	pthread_spin_lock(&kcp_server.kcp_lock);
	ret = ikcp_send(kcp, buf, len);
	pthread_spin_unlock(&kcp_server.kcp_lock);
	if(ret != 0)
	{
		log_error("ikcp_send error kcp:%p buf:%p ret:%d", kcp, buf, ret);
		return -1;
	}

	return len;
}

static int _kcp_send_pkt(ikcpcb * kcp, char *buf, size_t len)
{
	uint32_t net_len;
	net_len = htonl(len);
	if(_kcp_send_all(kcp, (char *)&net_len, 4) != 4)
		return -1;
	if(_kcp_send_all(kcp, buf, len) != len)
		return -1;

	return 0;
}

int kcp_send_data_safe(struct kcp_server_client *client, char *buf, size_t len)
{
	if(client->kcp_context)
		return _kcp_send_pkt(client->kcp_context, buf, len);
	else
		return -1;
}

static int _check_recv_pkt(struct kcp_server_client *kcp_client)
{
	int pkt_len;
	uint32_t npkt_len;

	if(get_queue_data_count(&kcp_client->recv_queue) > 4)
	{
		read_data(&kcp_client->recv_queue, &npkt_len, 4);
		pkt_len = ntohl(npkt_len);
		if(pkt_len > 0)
		{
			if(get_queue_data_count(&kcp_client->recv_queue) >= 4 + pkt_len)
			{
				log_info("kcp package:%d", pkt_len);
				dequeue_data(&kcp_client->recv_queue, &npkt_len, 4);
				dequeue_data(&kcp_client->recv_queue, kcp_client->queue_buf, pkt_len);
				server_on_pkg((struct server_client *)kcp_client->priv, kcp_client->queue_buf, pkt_len);
				return 0;
			}
		}else{
			log_error("kcp recv len is invalid.");
		}
	}
	return -1;
}

static void _kcp_recvdata(struct kcp_server_client *kcp_client) {
	int recv_count;

	do {
		pthread_spin_lock(&kcp_server.kcp_lock);
		recv_count = ikcp_recv(kcp_client->kcp_context,
			kcp_client->recv_buf, BUFLEN);
		pthread_spin_unlock(&kcp_server.kcp_lock);
		// 没有收到包就退出
		if(recv_count > 0)
		{
			enqueue_data(&kcp_client->recv_queue, kcp_client->recv_buf, recv_count);
		}

	} while(recv_count > 0);

	while(_check_recv_pkt(kcp_client) == 0);
}

static void *_kcp_server_thread(void *opaque) {
	int recv_count;
	char *udp_buf;
	char *kcp_buf;
	uint32_t client_id;
	socklen_t addr_len;
	struct sockaddr_in form_addr;
	struct kcp_server_client *client;

	addr_len = sizeof(form_addr);
	udp_buf = calloc(2000, 1);
	if(udp_buf == NULL)
	{
		log_error("[%s] calloc error.", __func__);
		exit(-1);
	}
	kcp_buf = calloc(BUFLEN, 1);
	if(kcp_buf == NULL)
	{
		log_error("[%s] calloc error.", __func__);
		exit(-1);
	}

	while (1) {
		recv_count = recvfrom(kcp_server.sockfd, udp_buf, 2000, 0, (struct sockaddr *)&form_addr, &addr_len);
		if (recv_count <= 0)
		{
			log_info("[%s] udp exit.", __func__);
			break;
		}
		client_id = ntohl(*(uint32_t *)udp_buf);
		// log_info("udp recv:%d pck id:%u", recv_count, client_id);
		if(g_hash_table_contains(kcp_server.client_table, &client_id))
		{
			/* 定向投送 */
			client = (struct kcp_server_client *)g_hash_table_lookup(
				kcp_server.client_table, &client_id);
		}else{
			if(server_has_client(client_id))
			{
				client = _new_client(form_addr.sin_addr.s_addr,
					form_addr.sin_port, form_addr.sin_family, client_id);
				client->client_id = client_id;
				pthread_spin_lock(&kcp_server.table_lock);
				g_hash_table_insert(kcp_server.client_table, &client->client_id, client);
				pthread_spin_unlock(&kcp_server.table_lock);
				kcp_server_client_connect(client, client->client_id);
			}else{
				log_warning("no client, client_id:%u", client_id);
			}
		}
		// 如果 p1收到udp，则作为下层协议输入到kcp
		pthread_spin_lock(&kcp_server.kcp_lock);
		ikcp_input(client->kcp_context, udp_buf, recv_count);
		pthread_spin_unlock(&kcp_server.kcp_lock);
		_kcp_recvdata(client);
	}
	free(udp_buf);
	free(kcp_buf);
	return NULL;
}

static void _foreach_update(gpointer key, gpointer value, gpointer user_data)
{
	struct kcp_server_client *client = (struct kcp_server_client *)value;
	pthread_spin_lock(&kcp_server.kcp_lock);
	ikcp_update(client->kcp_context, iclock());
	pthread_spin_unlock(&kcp_server.kcp_lock);
}

static void _kcp_update() {
	pthread_spin_lock(&kcp_server.table_lock);
	g_hash_table_foreach(kcp_server.client_table, _foreach_update, NULL);
	pthread_spin_unlock(&kcp_server.table_lock);
}

static void *_kcp_update_thread(void *opaque)
{
	while(1)
	{
		isleep(1);
		_kcp_update();
	}
	return NULL;
}

int kcp_server_init(char *ip, uint16_t port) {
 	int ret;
	pthread_t p1, p2;

	log_info("kcp server init.");
	pthread_spin_init(&kcp_server.table_lock, PTHREAD_PROCESS_SHARED);
	pthread_spin_init(&kcp_server.kcp_lock, PTHREAD_PROCESS_SHARED);
	kcp_server.client_table = g_hash_table_new(g_int_hash, g_int_equal);
 
    kcp_server.sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if(kcp_server.sockfd < 0)
    {
        printf("error in socket create");
        exit(-1);
    }

	struct sockaddr_in bind_addr;
	bind_addr.sin_port = htons(port);
	bind_addr.sin_addr.s_addr = inet_addr(ip);
	bind_addr.sin_family = PF_INET;
	ret = bind(kcp_server.sockfd, (struct sockaddr *)&bind_addr, sizeof(struct sockaddr));
	if(ret != 0)
	{
        printf("error in socket bind");
		exit(-1);
	}

	pthread_create(&p1, NULL, _kcp_server_thread, NULL);
	pthread_create(&p2, NULL, _kcp_update_thread, NULL);
	pthread_detach(p1);
	pthread_detach(p2);
	return 0;
}

void kcp_server_client_destory(struct kcp_server_client * client)
{
	ikcp_release(client->kcp_context);
	client->kcp_context = NULL;
	free(client);
}