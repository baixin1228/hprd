#ifndef __NET_CLIENT_H__
#define __NET_CLIENT_H__

#include "net/tcp_client.h"
#include "net/kcp_client.h"

struct client_net {
	bool kcp_enable;

	void (*pkt_callback)(char *buf, size_t len);
};

int client_net_init(char *ip, uint16_t port);
int client_kcp_connect(char *ip, uint16_t port, uint32_t kcp_id);
bool client_kcp_active();
int client_net_bind_pkg_cb(void (*cb)(char *buf, size_t len));

int client_send_data(char *buf, size_t len);
void client_on_pkg(char *buf, size_t len);
void client_net_release();

#endif