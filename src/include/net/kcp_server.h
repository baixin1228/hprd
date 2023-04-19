#ifndef __KCP_SERVER_H__
#define __KCP_SERVER_H__
#include <pthread.h>
#include <arpa/inet.h>

#include "ikcp.h"
#include "queue.h"
#include "net/net_util.h"

struct kcp_server_client{
	uint32_t client_id;
	uint32_t nip;
	uint16_t nport;
	uint16_t family;
	ikcpcb *kcp_context;
	struct data_queue recv_queue;
	char recv_buf[BUFLEN];
	char queue_buf[BUFLEN];
	char send_buf[BUFLEN];
	time_t last_active;
	void *priv;
};

int kcp_server_init(char *ip, uint16_t port);
void kcp_bradcast(char *buf, uint32_t len);
struct kcp_server_client *kcp_create_client_connect(uint32_t nip, uint16_t nport, uint32_t client_id);
int kcp_server_send_pkt(struct kcp_server_client *client, char *buf, size_t len);

#endif