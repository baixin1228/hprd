#ifndef __NET_SERVER_H__
#define __NET_SERVER_H__

#include "net/tcp_server.h"
#include "net/kcp_server.h"

struct server_client {
		bool kcp_enable;
		/* tcp client */
		struct tcp_server_client *tcp_server_client;
		/* tcp client */
		struct kcp_server_client *kcp_server_client;
};

struct server_net {
	void (*pkt_callback)(struct server_client *client, char *buf, size_t len);
	void (*connect_callback)(struct server_client *client);
};

int server_net_init(char *ip, uint16_t port, bool kcp);
int server_net_bind_pkg_cb(void (*cb)(struct server_client *client,
	char *buf, size_t len));
int server_net_bind_connect_cb(void (*cb)(struct server_client *client));
int server_send_pkt(struct server_client *client, char *buf, size_t len);
void server_bradcast_data(char *buf, size_t len);
void tcp_server_client_connect(struct tcp_server_client *tcp_client);
bool server_has_client(uint32_t client_id);
void kcp_server_client_connect(struct kcp_server_client *kcp_client, uint32_t client_id);
uint32_t server_client_getid(struct server_client *client);
uint32_t server_client_newid();
void server_on_pkg(struct server_client *ser_client, char *buf, size_t len);
void server_net_release();

#endif