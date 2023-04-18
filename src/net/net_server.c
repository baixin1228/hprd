#include "util.h"
#include "net/net_server.h"

struct server_net *ser_net = NULL;
int server_net_init(char *ip, uint16_t port, bool kcp)
{
	ser_net = calloc(sizeof(*ser_net), 1);
	if(ser_net == NULL)
	{
		log_error("[%s] calloc error.", __func__);
    	return -1;
	}

	if(tcp_server_init(ip, port) != 0)
		return -1;

	if(kcp)
	{
		if(kcp_server_init(ip, port) != 0)
			return -1;
	}

	return 0;
}

int server_net_bind_pkg_cb(void (*cb)(struct server_client *client,
	char *buf, size_t len))
{
	if(ser_net == NULL)
	{
		log_error("[%s] ser_net not init.", __func__);
		return -1;
	}

	ser_net->pkt_callback = cb;
	return 0;
}

int server_net_bind_connect_cb(void (*cb)(struct server_client *client))
{
	if(ser_net == NULL)
	{
		log_error("[%s] ser_net not init.", __func__);
		return -1;
	}

	ser_net->connect_callback = cb;
	return 0;
}

int server_send_data(struct server_client *client, char *buf, size_t len)
{
	if(ser_net == NULL)
	{
		log_error("[%s] ser_net not init.", __func__);
		return -1;
	}

	/* kcp first */
	if (client->kcp_enable)
	{
		return kcp_send_data_safe(client->kcp_server_client, buf, len);
	}else{
		return tcp_send_data_safe(client->tcp_server_client, buf, len);
	}

	return -1;
}

struct bradcast_data{
	char *buf;
	size_t len;
};
static void _foreach_bradcast(gpointer key, gpointer value, gpointer user_data)
{
	struct server_client *ser_client;
	struct bradcast_data *data = (struct bradcast_data *)user_data;
	struct tcp_server_client *tcp_client = (struct tcp_server_client *)value;
	if(tcp_client == NULL)
	{
		log_error("tcp_client is null.", __func__);
		return;
	}

	ser_client = (struct server_client *)tcp_client->priv;

	if(ser_client == NULL)
	{
		log_error("ser_client is null.", __func__);
		return;
	}
	/* kcp first */
	if (ser_client->kcp_enable)
	{
		if(ser_client->kcp_server_client)
			kcp_send_data_safe(ser_client->kcp_server_client, data->buf,
				data->len);
		else
			log_error("[%s] not find kcp_server_client.", __func__);
	}else{
		tcp_send_data_unsafe(tcp_client, data->buf, data->len);
	}
}

void server_bradcast_data(char *buf, size_t len)
{
	struct bradcast_data data;
	data.buf = buf;
	data.len = len;

	if(ser_net == NULL)
	{
		log_error("[%s] ser_net not init.", __func__);
		return;
	}

	tcp_foreach_client(_foreach_bradcast, &data);
	return;
}

void tcp_server_client_connect(struct tcp_server_client *tcp_client)
{
	struct server_client *ser_client;
	if(ser_net == NULL)
	{
		log_error("[%s] ser_net not init.", __func__);
		return;
	}
	ser_client = calloc(sizeof(*ser_client), 1);
	if(ser_client == NULL)
	{
		log_error("[%s] calloc error.", __func__);
    	return;
	}
	ser_client->kcp_enable = false;
	ser_client->tcp_server_client = tcp_client;
	ser_client->tcp_server_client->priv = ser_client;

	if(ser_net->connect_callback)
	{
		ser_net->connect_callback(ser_client);
	}else{
		log_warning("connect_callback not find.");
	}
}

uint32_t server_client_getid(struct server_client *client)
{
	if(client->tcp_server_client)
	{
		return client->tcp_server_client->client_id;
	}

	return 0;
}

uint32_t server_client_newid()
{
	static uint32_t client_id = 0;
	client_id++;
	return client_id;
}

bool server_has_client(uint32_t client_id)
{
	struct tcp_server_client *tcp_client;
	tcp_client = tcp_find_client(client_id);
	if(tcp_client == NULL)
	{
		return false;
	}
	return true;
}

void kcp_server_client_connect(struct kcp_server_client *kcp_client, uint32_t client_id)
{
	struct server_client *ser_client;
	struct tcp_server_client *tcp_client;

	if(ser_net == NULL)
	{
		log_error("[%s] ser_net not init.", __func__);
		return;
	}
	tcp_client = tcp_find_client(client_id);
	if(tcp_client == NULL)
	{
		log_error("tcp_find_client find fail.");
		return;
	}
	ser_client = (struct server_client *)tcp_client->priv;

	ser_client->kcp_server_client = kcp_client;
	ser_client->kcp_server_client->priv = ser_client;
	ser_client->kcp_enable = true;

	if(ser_net->connect_callback)
	{
		ser_net->connect_callback(ser_client);
	}else{
		log_warning("connect_callback not find.");
	}
}

void server_on_pkg(struct server_client *ser_client, char *buf, size_t len)
{
	if(ser_net == NULL)
	{
		log_error("[%s] ser_net not init.", __func__);
		return;
	}

	if(ser_net->pkt_callback)
	{
		ser_net->pkt_callback(ser_client, buf, len);
	}else{
		log_warning("pkt_callback not find.");
	}
}

void server_net_release()
{
	if(ser_net == NULL)
	{
		return;
	}
	free(ser_net);
	ser_net = NULL;
}
