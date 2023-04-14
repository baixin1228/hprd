#include <stdio.h>
#include <stdlib.h>

#include "ikcp.h"

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <windows.h>
#elif !defined(__unix)
#define __unix
#endif

#ifdef __unix
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#endif

/* get system time */
static inline void _itimeofday(long *sec, long *usec)
{
	#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

/* get clock in millisecond 64 */
static inline IINT64 _iclock64(void)
{
	long s, u;
	IINT64 value;
	_itimeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

static inline IUINT32 _iclock()
{
	return (IUINT32)(_iclock64() & 0xfffffffful);
}

struct kcp_client{
	uint32_t nip;
	uint16_t nport;
	uint16_t family;
	ikcpcb *kcp_context;
};

struct {
	int sockfd;
	pthread_spinlock_t lock;
	GHashTable *client_table;
} kcp_server = {0};

int _udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	struct sockaddr_in send_addr;
	struct kcp_client *client = (struct kcp_client *)user;

	send_addr.sin_port = client->nport;
	send_addr.sin_addr.s_addr = client->nip;
	send_addr.sin_family = client->family;

	if(kcp_server.sockfd != -1)
		sendto(kcp_server.sockfd, buf, len, 0, &send_addr, sizeof(send_addr));

	return 0;
}

struct kcp_client *_new_client(uint32_t ip, uint16_t port, uint16_t family)
{
	struct kcp_client *ret = calloc(sizeof(struct kcp_client), 1);
	if(ret == NULL)
	{
		log_error("[%s] calloc error.", __func__);
		exit(-1);
	}
	ret->nip = ip;
	ret->nport = port;
	ret->family = family;

	// 创建两个端点的 kcp对象，第一个参数 conv是会话编号，同一个会话需要相同
	// 最后一个是 user参数，用来传递标识
	ret->kcp_context = ikcp_create(ip, (void*)ret);
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

int kcp_send(struct kcp_client *client, char *buf, size_t len)
{
	return ikcp_send(client->kcp_context, buf, len);
}

struct bradcast_data{
	char *buf;
	size_t len;
};
void foreach_key_value(gpointer key, gpointer value, gpointer user_data)
{
	struct bradcast_data *data = (struct bradcast_data *)user_data;
	kcp_send((struct kcp_client *)value, data->buf, data->len);
}

void kcp_bradcast_data(char *buf, uint32_t len) {
	struct bradcast_data data;
	data.buf = buf;
	data.len = len;
	pthread_spin_lock(&kcp_server.lock);
	g_hash_table_foreach(kcp_server.client_table, foreach_key_value, &data);
	pthread_spin_unlock(&kcp_server.lock);
}

void on_server_pkt(struct ep_event *ev, char *buf, size_t len);
void *_kcp_server_thread(void *opaque) {
	int recv_count;
	char *ucp_buf;
	char *kcp_buf;
	int addr_len;
	struct sockaddr_in form_addr;
	struct kcp_client *client;

	ucp_buf = calloc(2000, 1);
	if(ucp_buf == NULL)
	{
		log_error("[%s] calloc error.", __func__);
		exit(-1);
	}
	kcp_buf = calloc(2000, 1);
	if(kcp_buf == NULL)
	{
		log_error("[%s] calloc error.", __func__);
		exit(-1);
	}

	while (1) {
		recv_count = recvfrom(kcp_server.sockfd, ucp_buf, 2000, 0, (struct sockaddr *)&form_addr, &addr_len);
		if (recv_count < 0)
		{
			log_info("[%s] udp exit.", __func__);
			break;
		}

		if(g_hash_table_contains(kcp_server.client_table, &form_addr.sin_addr.s_addr))
		{
			client = *(struct kcp_client *)g_hash_table_lookup(
				kcp_server.client_table, &form_addr.sin_addr.s_addr);
		}else{
			client = _new_client(form_addr.sin_addr.s_addr, form_addr.sin_port,
				form_addr.sin_family);
			pthread_spin_lock(&kcp_server.lock);
			g_hash_table_insert(kcp_server.client_table, &client.nip, client);
			pthread_spin_unlock(&kcp_server.lock);
		}
		// 如果 p1收到udp，则作为下层协议输入到kcp
		ikcp_input(client->kcp_context, ucp_buf, recv_count);
		ikcp_update(client->kcp_context, _iclock());
		recv_count = ikcp_recv(client->kcp_context, kcp_buf, 2000);
		// 没有收到包就退出
		if(recv_count < 0)
		{
			log_info("[%s] kcp exit.", __func__);
			break;
		}
		on_server_pkt(client, kcp_buf, recv_count);
	}
	free(ucp_buf);
	free(kcp_buf);
}

int kcp_server_init(char *ip, uint16_t port) {
 	int ret;
	pthread_t p1;
    struct addrinfo client, *servinfo;

	pthread_spin_init(&kcp_server.lock, NULL);
	kcp_server.client_table = g_hash_table_new(g_int_hash, g_int_equal);

    memset(&client, 0, sizeof(client));
    client.ai_family = AF_INET;
    client.ai_socktype = SOCK_DGRAM;
    client.ai_protocol= IPPROTO_UDP;
 
    if(getaddrinfo(ip, port, &client, &servinfo) < 0)
    {
        printf("error in getaddrinfo");
        exit(-1);
    }
 
    kcp_server.sockfd = socket(servinfo->ai_family, servinfo->ai_socktype,
    	servinfo->ai_protocol);
    if(kcp_server.sockfd < 0)
    {
        printf("error in socket create");
        exit(-1);
    }

	struct sockaddr_in bind_addr;
	bind_addr.sin_port = htons(port);
	bind_addr.sin_addr.s_addr = inet_addr(ip);
	bind_addr.sin_family = servinfo->ai_family;
	ret = bind(kcp_server.sockfd, (struct sockaddr *)&bind_addr, sizeof(struct sockaddr));
	if(ret != 0)
	{
        printf("error in socket bind");
		exit(-1);
	}

	pthread_create(&p1, NULL, _kcp_server_thread, NULL);
	pthread_detach(p1);
	return 0;
}

void kcp_client_destory(struct kcp_client * client)
{
	ikcp_release(client->kcp_context);
	client->kcp_context = NULL;
	free(client);
}