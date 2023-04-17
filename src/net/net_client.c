#include <stdlib.h>
#include <malloc.h>

#include "util.h"
#include "net/net_client.h"
#include "net/tcp_client.h"
#include "net/kcp_client.h"

struct client_net *client_net = NULL;
int client_net_init(char *ip, uint16_t port)
{
	client_net = calloc(sizeof(*client_net), 1);
	if(client_net == NULL)
	{
		log_error("[%s] calloc error.", __func__);
    	return -1;
	}

	if(tcp_client_init(ip, port) != 0)
		return -1;

	return 0;
}

int client_kcp_connect(char *ip, uint16_t port, uint32_t kcp_id)
{
	if(client_net == NULL)
	{
		log_error("[%s] client_net not init.", __func__);
		return -1;
	}

	if(kcp_client_init(ip, port, kcp_id) != 0)
		return -1;
	client_net->kcp_enable = true;

	return 0;
}

int client_net_bind_pkg_cb(void (*cb)(char *buf, size_t len))
{
	if(client_net == NULL)
	{
		log_error("[%s] client_net not init.", __func__);
		return -1;
	}

	client_net->pkt_callback = cb;
	return 0;
}

int client_send_data(char *buf, size_t len)
{
	if(client_net == NULL)
	{
		log_error("[%s] client_net not init.", __func__);
		return -1;
	}
	if (client_net->kcp_enable)
	{
		return kcp_client_send_data(buf, len);
	}else{
		return tcp_client_send_data(buf, len);
	}
}

void client_on_pkg(char *buf, size_t len)
{
	if(client_net == NULL)
	{
		log_error("[%s] client_net not init.", __func__);
		return;
	}

	if(client_net->pkt_callback)
	{
		client_net->pkt_callback(buf, len);
	}else{
		log_warning("pkt_callback not find.");
	}
}

void client_net_release()
{
	if(client_net == NULL)
	{
		return;
	}
	if (client_net->kcp_enable)
	{
		kcp_client_release();
	}
	tcp_client_release();
	free(client_net);
	client_net = NULL;
}
