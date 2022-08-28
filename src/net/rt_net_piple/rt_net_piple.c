#include <malloc.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "util.h"
#include "rt_net_piple.h"
#include "rt_net_socket.h"

#define SERVER_MAX 1024
uint16_t server_idx = 0;
pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct rt_net_server *servers[SERVER_MAX];

int rt_net_init_server(char *addr, uint16_t port)
{
	int ret = -1;
	pthread_mutex_lock(&server_mutex);
	if(server_idx < SERVER_MAX)
	{
		servers[server_idx] = (struct rt_net_server *)calloc(1, sizeof(struct rt_net_server));
		if(servers[server_idx] != NULL)
		{
			pthread_mutex_init(&servers[server_idx]->accept_lock, NULL);
			ret = 0;
		}
	}

	if(ret == 0)
	{
		if(net_server_init(servers[server_idx], addr, port) != -1)
		{
			ret = server_idx;
			server_idx++;
		}else
			free(servers[server_idx]);
	}
	pthread_mutex_unlock(&server_mutex);

	return ret;
}

int rt_net_release_server(int server_id)
{
	int i;
	int ret = -1;
	struct rt_net_server *server;
	struct rt_net_client *client;
	pthread_mutex_lock(&server_mutex);
	if(server_idx < SERVER_MAX)
	{
		if(servers[server_id])
		{
			server = servers[server_id];
			for (i = 0; i < server->client_idx; ++i)
			{
				if(server->clients[i])
				{
					client = server->clients[i];
					net_client_release(client);
					free(client);
					server->clients[i] = NULL;
				}
			}
			net_server_release(server);
			free(server);

			servers[server_id] = NULL;
			ret = 0;
		}
	}
	pthread_mutex_unlock(&server_mutex);
	return ret;
}

uint32_t client_idx = 0;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
struct rt_net_client *clients[CLIENT_MAX];

int rt_net_init_client(char *addr, uint16_t port)
{
	int ret = -1;
	pthread_mutex_lock(&client_mutex);
	if(client_idx < CLIENT_MAX)
	{
		clients[client_idx] = (struct rt_net_client *)calloc(1, sizeof(struct rt_net_client));
		if(clients[client_idx] != NULL)
		{
			pthread_mutex_init(&clients[client_idx]->piple_open_lock, NULL);
			pthread_mutex_init(&clients[client_idx]->accept_piple_lock, NULL);
			pthread_cond_init(&clients[client_idx]->msg_signal, NULL);
			ret = 0;
		}
	}

	if(ret == 0)
	{
		if(net_client_init(clients[client_idx], addr, port) != -1)
		{
			ret = client_idx;
			client_idx++;
		}else
			free(clients[client_idx]);
	}
	pthread_mutex_unlock(&client_mutex);

	return ret;
}

static struct rt_net_client *_get_client(int server_id, int client_id)
{
	struct rt_net_client *client = NULL;
	struct rt_net_server *server = NULL;

	if(server_idx >= 0 && server_idx < SERVER_MAX)
	{
		if(servers[server_id])
		{
			server = servers[server_id];
		}
	}
	if(server == NULL)
	{
		func_error("not find server.");
	}

	if(client_idx >= 0 && client_idx < CLIENT_MAX)
	{
		if(server)
			client = server->clients[client_id];
		else
			client = clients[client_id];

		func_error("client %p.", client);
		return client;
	}
	return NULL;
}

int rt_net_release_client(int server_id, int client_id)
{
	int ret = -1;

	struct rt_net_client *client = NULL;
	struct rt_net_server *server = NULL;

	pthread_mutex_lock(&server_mutex);
	pthread_mutex_lock(&client_mutex);

	if(server_idx >= 0 && server_idx < SERVER_MAX)
	{
		if(servers[server_id])
		{
			server = servers[server_id];
		}
	}

	if(client_idx >= 0 && client_idx < CLIENT_MAX)
	{
		if(server)
		{
			client = server->clients[client_id];
			server->clients[client_id] = NULL;
		}else{
			client = clients[client_id];
			clients[client_id] = NULL;
		}

		if(client != NULL)
		{
			net_client_release(client);
			free(client);
			ret = 0;
		}
	}

	pthread_mutex_unlock(&client_mutex);
	pthread_mutex_unlock(&server_mutex);
	return ret;
}


int rt_net_server_accept(int server_id)
{
	int ret = -1;
	struct rt_net_server *server;
	if(server_idx < SERVER_MAX)
	{
		if(servers[server_id])
		{
			server = servers[server_id];
			pthread_mutex_lock(&server->accept_lock);
			clients[server->client_idx] = (struct rt_net_client *)calloc(1, sizeof(struct rt_net_client));
			if(clients[server->client_idx] != NULL)
			{
				pthread_mutex_init(&clients[server->client_idx]->piple_open_lock, NULL);
				pthread_mutex_init(&clients[server->client_idx]->accept_piple_lock, NULL);
				pthread_cond_init(&clients[server->client_idx]->msg_signal, NULL);
				ret = tcp_accept(server, clients[server->client_idx]);
				if(ret == 0)
				{
					ret = server->client_idx;
					server->client_idx++;
				}else{
					free(clients[server->client_idx]);
					clients[server->client_idx] = NULL;
				}
			}
			pthread_mutex_unlock(&server->accept_lock);
		}
	}
	return ret;
}

int rt_net_open_piple(
	int server_id,
	int client_id,
	enum RT_PIPLE_TYPE piple_type,
	uint8_t piple_id,
	void *(* recv_callback)(int piple_id, uint8_t *buf, uint16_t len))
{
	int ret = -1;
	struct piple_pkt pkt;
	struct rt_net_client *client = _get_client(server_id, client_id);
	struct rt_net_piple *piple;

	if(client)
	{
		pthread_mutex_lock(&client->piple_open_lock);

		client->piples[client->piple_idx] = calloc(1, sizeof(struct rt_net_piple));
		if(client->piples[client->piple_idx] == NULL)
			return -1;
		piple = client->piples[client->piple_idx];
		piple->id = client->piple_idx;
		piple->piple_type = piple_type;
		piple->recv_callback = recv_callback;
		pthread_mutex_init(&piple->send_lock, NULL);
		piple->state = PIPLE_OPENING;
		pkt.head.cmd = OPEN_PIPLE;
		pkt.head.piple_id = client->piple_idx;
		pkt.head.data_len = 0;
		tcp_send_pkt(client, (uint8_t *)&pkt, sizeof(pkt.head));
		while(pthread_cond_wait(&client->msg_signal, &client->piple_open_lock))
		{
			if(piple->state == PIPLE_OPENED)
			{
				ret = 0;
				break;
			}
			if(piple->state == PIPLE_ERROR)
			{
				break;
			}
		}
		pthread_mutex_unlock(&client->piple_open_lock);
	}
	return ret;
}

int rt_net_piple_accept(
	int server_id,
	int client_id)
{
	int i;
	if(server_id < 0)
	{
		func_error("server id is null.");
		return -1;
	}

	struct rt_net_client *client = _get_client(server_id, client_id);

	while(client)
	{
		pthread_mutex_lock(&client->accept_piple_lock);
		pthread_cond_wait(&client->msg_signal, &client->accept_piple_lock);
		for (i = 0; i < PIPLE_MAX; ++i)
		{
			if(client->piples[i] != NULL &&
				client->piples[i]->state == PIPLE_OPENING)
			{
				return i;
			}
		}
		pthread_mutex_unlock(&client->accept_piple_lock);
	}
	func_error("not find client.");
	return -1;
}

int rt_net_piple_bind(
	int server_id,
	int client_id,
	uint8_t piple_id,
	void *(* recv_callback)(int piple_id, uint8_t *buf, uint16_t len))
{
	if(server_id < 0)
	{
		func_error("server id is null.");
		return -1;
	}

	int ret = -1;
	struct piple_pkt pkt;
	struct rt_net_piple *piple;
	struct rt_net_client *client = _get_client(server_id, client_id);

	if(client)
	{
		if(client->piples[piple_id] == NULL)
			return -1;
		piple = client->piples[piple_id];
		pthread_mutex_lock(&piple->send_lock);
		piple->id = client->piple_idx;
		piple->recv_callback = recv_callback;
		pthread_mutex_init(&piple->send_lock, NULL);
		pkt.head.cmd = BIND_PIPLE;
		pkt.head.piple_id = piple_id;
		pkt.head.data_len = 0;
		tcp_send_pkt(client, (uint8_t *)&pkt, sizeof(pkt.head));
		while(pthread_cond_wait(&client->msg_signal, &piple->send_lock))
		{
			if(piple->state == PIPLE_OPENED)
			{
				ret = 0;
				break;
			}
			if(piple->state == PIPLE_ERROR)
			{
				func_error("bind piple response error.");
				break;
			}
		}
		pthread_mutex_unlock(&piple->send_lock);
		return ret;
	}
	func_error("not find client.");
	return -1;
}

bool rt_net_has_piple(
	int server_id,
	int client_id,
	uint8_t piple_id)
{
	struct rt_net_client *client = _get_client(server_id, client_id);
	if(client)
	{
		if(client->piples[piple_id] != NULL)
		{
			return true;
		}
		return false;
	}
	return false;
}

int rt_net_close_piple(
	int server_id,
	int client_id,
	int piple_id)
{
	int ret;
	struct rt_net_piple *piple;
	struct rt_net_client *client = _get_client(server_id, client_id);
	if(client)
	{
		if(client->piples[piple_id] == NULL)
		{
			piple = client->piples[piple_id];
			if(piple->state == PIPLE_OPENED)
			{
				pthread_mutex_lock(&piple->send_lock);
				piple->send_pkt.head.cmd = CLOSE_PIPLE;
				piple->send_pkt.head.piple_id = piple_id;
				piple->send_pkt.head.data_len = 0;
				ret = tcp_send_pkt(client, (uint8_t *)&piple->send_pkt, sizeof(piple->send_pkt.head));
				while(pthread_cond_wait(&client->msg_signal, &piple->send_lock))
				{
					if(piple->state == PIPLE_CLOSED)
					{
						ret = 0;
						break;
					}
					if(piple->state == PIPLE_ERROR)
					{
						func_error("close piple response error.");
						break;
					}
				}
				pthread_mutex_lock(&piple->send_lock);
				return ret;
			}
			func_error("piple not open.");
			return -1;
		}
		func_error("not find piple.");
		return -1;
	}
	func_error("not find client.");
	return -1;
}

int rt_net_send(
	int server_id,
	int client_id,
	int piple_id,
	const uint8_t *buf,
	size_t len)
{
	int ret;
	struct rt_net_piple *piple;
	struct rt_net_client *client = _get_client(server_id, client_id);
	if(client)
	{
		if(client->piples[piple_id] == NULL)
		{
			piple = client->piples[piple_id];
			if(piple->state == PIPLE_OPENED)
			{
				pthread_mutex_lock(&piple->send_lock);
				piple->send_pkt.head.cmd = PIPLE_DATA;
				piple->send_pkt.head.piple_id = piple_id;
				memcpy(piple->send_pkt.data, buf, len);
				piple->send_pkt.head.data_len = len;
				ret = tcp_send_pkt(client, (const uint8_t *)&piple->send_pkt, sizeof(piple->send_pkt.head));
				pthread_mutex_lock(&piple->send_lock);
				return ret;
			}
			func_error("piple not open.");
			return -1;
		}
		func_error("not find piple.");
		return -1;
	}
	func_error("not find client.");
	return -1;
}

void _on_data_recv(struct rt_net_client *client, uint8_t *buf, uint16_t len)
{
	struct piple_pkt *pkt = (struct piple_pkt *)buf;
	if(len >= sizeof(pkt->head))
	{
		switch(pkt->head.cmd)
		{
			case OPEN_PIPLE:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					client->piples[client->piple_idx] = calloc(1, sizeof(struct rt_net_piple));
					if(client->piples[client->piple_idx] == NULL)
					{
						pkt->head.cmd = OPEN_REP_ERROR;
					}else{
						pkt->head.cmd = OPEN_REP_SUCCESS;
					}
				}else{
					pkt->head.cmd = OPEN_REP_SUCCESS;
				}
				if(pkt->head.cmd == OPEN_REP_SUCCESS)
				{
					client->piples[client->piple_idx]->state = PIPLE_OPENING;
					client->piples[client->piple_idx]->piple_type = pkt->head.piple_type;
				}
				pthread_cond_signal(&client->msg_signal);
				tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case OPEN_REP_ERROR:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					func_error("OPEN_REP_ERROR not find piple, piple id:%d", pkt->head.piple_id);
				}else{
					client->piples[pkt->head.piple_id]->state = PIPLE_ERROR;
				}
				pthread_cond_signal(&client->msg_signal);
				break;
			}
			case OPEN_REP_SUCCESS:
			{
				pthread_cond_signal(&client->msg_signal);
				break;
			}
			case BIND_PIPLE:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					func_error("BIND_PIPLE not find piple, piple id:%d", pkt->head.piple_id);
					pkt->head.cmd = BIND_REP_ERROR;
				}else{
					client->piples[client->piple_idx]->state = PIPLE_OPENED;
					pkt->head.cmd = BIND_REP_SUCCESS;
				}
				pthread_cond_signal(&client->msg_signal);
				tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case BIND_REP_ERROR:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					func_error("BIND_REP_ERROR not find piple, piple id:%d", pkt->head.piple_id);
				}else{
					client->piples[pkt->head.piple_id]->state = PIPLE_ERROR;
				}
				pthread_cond_signal(&client->msg_signal);
				break;
			}
			case BIND_REP_SUCCESS:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					func_error("BIND_REP_SUCCESS not find piple, piple id:%d", pkt->head.piple_id);
				}else{
					client->piples[client->piple_idx]->state = PIPLE_OPENED;
				}
				pthread_cond_signal(&client->msg_signal);
				break;
			}
			case PIPLE_DATA:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					func_error("PIPLE_DATA not find piple, piple id:%d", pkt->head.piple_id);
					pkt->head.cmd = CLOSE_PIPLE_REP_ERROR;
				}else{
					if(client->piples[client->piple_idx]->state == PIPLE_OPENED)
					{
						if(client->piples[client->piple_idx]->recv_callback)
							client->piples[client->piple_idx]->recv_callback(client->piples[client->piple_idx]->id, pkt->data, pkt->head.data_len);
						else
						log_warning("PIPLE_DATA recv_callback is null, Data will be discarded.");
					}else{
						func_error("PIPLE_DATA piple is not open.");
					}
				}
				break;
			}
			case CLOSE_PIPLE:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					func_error("CLOSE_PIPLE not find piple, piple id:%d", pkt->head.piple_id);
					pkt->head.cmd = CLOSE_PIPLE_REP_ERROR;
				}else{
					client->piples[client->piple_idx]->state = PIPLE_CLOSED;
					pkt->head.cmd = CLOSE_PIPLE_REP_SUCCESS;
				}
				pthread_cond_signal(&client->msg_signal);
				tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case CLOSE_PIPLE_REP_ERROR:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					func_error("CLOSE_PIPLE_REP_ERROR not find piple, piple id:%d", pkt->head.piple_id);
				}else{
					client->piples[client->piple_idx]->state = PIPLE_ERROR;
				}
				pthread_cond_signal(&client->msg_signal);
				break;
			}
			case CLOSE_PIPLE_REP_SUCCESS:
			{
				if(client->piples[pkt->head.piple_id] == NULL)
				{
					func_error("CLOSE_PIPLE_REP_SUCCESS not find piple, piple id:%d", pkt->head.piple_id);
				}else{
					client->piples[client->piple_idx]->state = PIPLE_CLOSED;
				}
				pthread_cond_signal(&client->msg_signal);
				break;
			}
		}
	}else{
		func_error("pkt is too small.");
	}
}