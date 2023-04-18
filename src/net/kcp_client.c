#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "util.h"
#include "queue.h"
#include "net/ikcp.h"
#include "net/isleep.h"
#include "net/net_util.h"
#include "net/kcp_client.h"
#include "net/net_client.h"

struct {
	int sockfd;
	uint32_t kcp_id;
	pthread_spinlock_t kcp_lock;
	struct sockaddr_in ser_addr;
	struct data_queue recv_queue;
	char recv_buf[BUFLEN];
	ikcpcb *kcp_context;
} kcp_client = {0};

static inline int _kcp_send_all(ikcpcb * kcp, char *buf, size_t len)
{
	int ret;
	pthread_spin_lock(&kcp_client.kcp_lock);
	ret = ikcp_send(kcp, buf, len);
	pthread_spin_unlock(&kcp_client.kcp_lock);
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

static int _check_recv_pkt()
{
	int pkt_len;
	char *buf_ptr;
	buf_ptr = queue_get_read_ptr(&kcp_client.recv_queue);
	if(get_queue_data_count(&kcp_client.recv_queue) > 4)
	{
		pkt_len = ntohl(*(uint32_t *)(buf_ptr));
		if(pkt_len > 0)
		{
			if(get_queue_data_count(&kcp_client.recv_queue) >= 4 + pkt_len)
			{
				log_info("kcp package:%d", pkt_len);
				client_on_pkg(buf_ptr + 4, pkt_len);
				queue_tail_point_forward(&kcp_client.recv_queue, 4 + pkt_len);
				return 0;
			}
		}else{
			log_error("kcp recv len is invalid.");
		}
	}
	return -1;
}

static void _kcp_recvdata() {
	int recv_count;

	do {
		pthread_spin_lock(&kcp_client.kcp_lock);
		recv_count = ikcp_recv(kcp_client.kcp_context,
			kcp_client.recv_buf, BUFLEN);
		pthread_spin_unlock(&kcp_client.kcp_lock);
		// 没有收到包就退出
		if(recv_count > 0)
		{
			enqueue_data(&kcp_client.recv_queue, kcp_client.recv_buf, recv_count);
		}

	} while(recv_count > 0);

	while(_check_recv_pkt() == 0);
}

static void *_recv_thread(void *oq) {
	char *udp_buf;
	int recv_count;
	socklen_t addr_len;
	struct sockaddr_in form_addr;

	udp_buf = calloc(2000, 1);
	if(udp_buf == NULL)
	{
		log_error("[%s] calloc error.", __func__);
		exit(-1);
	}
	while(1)
	{
		recv_count = recvfrom(kcp_client.sockfd, udp_buf, 2000, 0, (struct sockaddr *)&form_addr, &addr_len);
		if (recv_count < 0)
		{
			log_info("[%s] udp exit.", __func__);
			break;
		}
		ikcp_input(kcp_client.kcp_context, udp_buf, recv_count);
		_kcp_recvdata();
	}

	log_warning("kcp_recv_thread exit.");
	return NULL;
}

static int _udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	// log_info("udp send %s %d len:%d\n", inet_ntoa(kcp_client.ser_addr.sin_addr), ntohs(kcp_client.ser_addr.sin_port), len);
	if(kcp_client.sockfd != -1)
		sendto(kcp_client.sockfd, buf, len, 0,
			(struct sockaddr *)&kcp_client.ser_addr,
			sizeof(kcp_client.ser_addr));

	return 0;
}

static void *_kcp_update(void *oq) {
	while(1)
	{
		isleep(1);
		pthread_spin_lock(&kcp_client.kcp_lock);
		ikcp_update(kcp_client.kcp_context, iclock());
		pthread_spin_unlock(&kcp_client.kcp_lock);
	}
	return NULL;
}

int kcp_client_init(char *ip, uint16_t port, uint32_t kcp_id)
{
	pthread_t p1, p2;
	kcp_client.ser_addr.sin_port = htons(port);
	kcp_client.ser_addr.sin_addr.s_addr = inet_addr(ip);
	kcp_client.ser_addr.sin_family = PF_INET;
	kcp_client.kcp_id = kcp_id;

	pthread_spin_init(&kcp_client.kcp_lock, PTHREAD_PROCESS_SHARED);

    kcp_client.sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if(kcp_client.sockfd < 0)
    {
        printf("error in socket create");
        exit(-1);
    }
	// 创建两个端点的 kcp对象，第一个参数 conv是会话编号，同一个会话需要相同
	// 最后一个是 user参数，用来传递标识
	kcp_client.kcp_context = ikcp_create(htonl(kcp_id), NULL);
	log_info("ikcp_create, client_id:%u", kcp_id);
	// 设置kcp的下层输出，这里为 _udp_output，模拟udp网络输出函数
	kcp_client.kcp_context->output = _udp_output;

	// 配置窗口大小：平均延迟200ms，每20ms发送一个包，
	// 而考虑到丢包重发，设置最大收发窗口为128
	ikcp_wndsize(kcp_client.kcp_context, 128, 128);

	// 启动快速模式
	// 第二个参数 nodelay-启用以后若干常规加速将启动
	// 第三个参数 interval为内部处理时钟，默认设置为 10ms
	// 第四个参数 resend为快速重传指标，设置为2
	// 第五个参数 为是否禁用常规流控，这里禁止
	ikcp_nodelay(kcp_client.kcp_context, 2, 10, 2, 1);
	kcp_client.kcp_context->rx_minrto = 10;
	kcp_client.kcp_context->fastresend = 1;

	pthread_create(&p1, NULL, _recv_thread, NULL);
	pthread_create(&p2, NULL, _kcp_update, NULL);
	pthread_detach(p1);
	pthread_detach(p2);
	return 0;
}

int kcp_client_send_data(char *buf, size_t len)
{
	if(kcp_client.kcp_context)
		return _kcp_send_pkt(kcp_client.kcp_context, buf, len);
	else
		return -1;
}

void kcp_client_release()
{
	if(kcp_client.kcp_context)
	{
		ikcp_release(kcp_client.kcp_context);
		kcp_client.kcp_context = NULL;
	}
	if(kcp_client.sockfd != -1) {
		close(kcp_client.sockfd);
	}
	kcp_client.sockfd = -1;
}