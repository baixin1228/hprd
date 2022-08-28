#include <malloc.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "util.h"
#include "rt_net_piple.h"
#include "rt_net_socket.h"

pthread_mutex_t server_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

Server rt_net_init_server(char *addr, uint16_t port)
{
	Server ret = NULL;
	struct rt_net_server *server;
	pthread_mutex_lock(&server_lock);

	server = (struct rt_net_server *)calloc(1, sizeof(struct rt_net_server));
	if(server != NULL)
	{
		pthread_mutex_init(&server->accept_lock, NULL);
		if(net_server_init(server, addr, port) != -1)
		{
			ret = (Server)server;
		}else{
			free(server);
		}
	}else{
		func_error("calloc fail.");
	}

	pthread_mutex_unlock(&server_lock);

	return ret;
}

int rt_net_release_server(Server server_fd)
{
	int i;
	int ret = -1;
	struct rt_net_server *server = (struct rt_net_server *)server_fd;;
	struct rt_net_client *client;

	pthread_mutex_lock(&server_lock);
	if(server)
	{
		for (i = 0; i < server->client_idx; ++i)
		{
			if(server->clients[i])
			{
				client = server->clients[i];
				server->clients[i] = NULL;
				net_client_release(client);
				free(client);
			}
		}
		net_server_release(server);
		free(server);
		ret = 0;
	}else{
		func_error("Not find server.");
	}
	pthread_mutex_unlock(&server_lock);
	return ret;
}

Client rt_net_init_client(char *addr, uint16_t port)
{
	Client ret = NULL;
	struct rt_net_client *client;

	pthread_mutex_lock(&client_lock);
	client = (struct rt_net_client *)calloc(1, sizeof(struct rt_net_client));
	if(client != NULL)
	{
		pthread_mutex_init(&client->piple_open_lock, NULL);
		pthread_mutex_init(&client->accept_piple_lock, NULL);
		pthread_cond_init(&client->msg_signal, NULL);
		client->id = -1;
		client->server = NULL;
		if(net_client_init(client, addr, port) != -1)
		{
			ret = (Client)client;
		}else{
			free(client);
		}
	}else{
		func_error("calloc fail.");
	}
	pthread_mutex_unlock(&client_lock);

	return ret;
}

int rt_net_release_client(Client client_fd)
{
	int ret = -1;
	struct rt_net_server *server = NULL;
	struct rt_net_client *client = (struct rt_net_client *)client_fd;

	if(client != NULL)
	{
		pthread_mutex_lock(&client_lock);
		net_client_release(client);
		if(client->server)
		{
			pthread_mutex_lock(&server_lock);
			server = (struct rt_net_server *)client->server;
			if(server->clients[client->id])
				server->clients[client->id] = NULL;
			pthread_mutex_unlock(&server_lock);
		}
		free(client);
		pthread_mutex_unlock(&client_lock);
		ret = 0;
	}else{
		func_error("Not find client.");
	}

	return ret;
}

Client rt_net_server_accept(Server server_fd)
{
	Client ret = NULL;
	struct rt_net_client *client;
	struct rt_net_server *server = (struct rt_net_server *)server_fd;

	if(server)
	{
		pthread_mutex_lock(&server->accept_lock);
		client = (struct rt_net_client *)calloc(1, sizeof(struct rt_net_client));
		if(client != NULL)
		{
			pthread_mutex_init(&client->piple_open_lock, NULL);
			pthread_mutex_init(&client->accept_piple_lock, NULL);
			pthread_cond_init(&client->msg_signal, NULL);
			if(tcp_accept(server, client) == 0)
			{
				server->clients[server->client_idx] = client;
				client->id = server->client_idx;
				client->server = server_fd;
				server->client_idx++;
				ret = (Client)client;
			}else{
				free(client);
			}
		}else{
			func_error("calloc fail.");
		}
		pthread_mutex_unlock(&server->accept_lock);
	}else{
		func_error("Not find server.");
	}

	return ret;
}

Piple rt_net_open_piple(
	Client client_fd,
	enum RT_PIPLE_TYPE piple_type,
	uint8_t piple_id,
	void *(* recv_callback)(Piple piple_fd, uint8_t *buf, uint16_t len))
{
	Piple ret = NULL;
	struct piple_pkt pkt;
	struct rt_net_piple *piple;
	struct rt_net_client *client = (struct rt_net_client *)client_fd;

	if(client)
	{
		pthread_mutex_lock(&client->piple_open_lock);

		piple = calloc(1, sizeof(struct rt_net_piple));
		if(piple != NULL)
		{
			client->piples[client->piple_idx] = piple;
			piple->id = client->piple_idx;
			client->piple_idx++;

			piple->piple_type = piple_type;
			piple->recv_callback = recv_callback;
			piple->state = PIPLE_OPENING;

			pthread_mutex_init(&piple->send_lock, NULL);
			pkt.head.cmd = OPEN_PIPLE;
			pkt.head.piple_id = piple->id;
			pkt.head.data_len = 0;
			tcp_send_pkt(client, (uint8_t *)&pkt, sizeof(pkt.head));
			while(pthread_cond_wait(&client->msg_signal, &client->piple_open_lock))
			{
				if(piple->state == PIPLE_OPENED)
				{
					ret = (Piple)piple;
					break;
				}
				if(piple->state == PIPLE_ERROR)
				{
					func_error("Piple open error.");
					break;
				}
			}
		}else{
			func_error("calloc fail.");
		}
		pthread_mutex_unlock(&client->piple_open_lock);
	}else{
		func_error("Not find client.");
	}
	return ret;
}

Piple rt_net_piple_accept(Client client_fd)
{
	int i;
	Piple ret = NULL;
	struct rt_net_client *client = (struct rt_net_client *)client_fd;

	if(client)
	{
		pthread_mutex_lock(&client->accept_piple_lock);
		pthread_cond_wait(&client->msg_signal, &client->accept_piple_lock);
		for (i = 0; i < PIPLE_MAX; ++i)
		{
			if(client->piples[i] != NULL &&
				client->piples[i]->state == PIPLE_OPENING)
			{
				return (Piple)client->piples[i];
			}
		}
		pthread_mutex_unlock(&client->accept_piple_lock);
	}else{
		func_error("not find client.");
	}

	return ret;
}

int rt_net_piple_bind(
	Piple piple_fd,
	void *(* recv_callback)(Piple piple_fd, uint8_t *buf, uint16_t len))
{
	int ret = -1;
	struct piple_pkt pkt;
	struct rt_net_client *client;
	struct rt_net_piple *piple = (struct rt_net_piple *)piple_fd;

	if(piple)
	{
		client = (struct rt_net_client *)piple->client;
		if(client)
		{
			piple->id = client->piple_idx;
			piple->recv_callback = recv_callback;

			pthread_mutex_lock(&piple->send_lock);
			pkt.head.cmd = BIND_PIPLE;
			pkt.head.piple_id = piple->id;
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
		}else{
			func_error("not find client.");
		}
	}else{
		func_error("not find piple.");
	}
	return ret;
}

bool rt_net_has_piple(
	Client client_fd,
	uint8_t piple_id)
{
	struct rt_net_client *client = (struct rt_net_client *)client_fd;
	if(client)
	{
		if(client->piples[piple_id] != NULL && client->piples[piple_id]->state == PIPLE_OPENED)
		{
			return true;
		}
	}else{
		func_error("not find client.");
	}
	return false;
}

Piple rt_net_get_piple(
	Client client_fd,
	uint8_t piple_id)
{
	struct rt_net_client *client = (struct rt_net_client *)client_fd;
	if(client)
	{
		if(client->piples[piple_id] != NULL && client->piples[piple_id]->state == PIPLE_OPENED)
		{
			return (Piple)client->piples[piple_id];
		}
	}else{
		func_error("not find client.");
	}
	return NULL;
}

int rt_net_close_piple(
	Client client_fd,
	Piple piple_fd)
{
	int ret;
	struct rt_net_client *client;
	struct rt_net_piple *piple = (struct rt_net_piple *)piple_fd;

	if(piple)
	{
		if(piple->state == PIPLE_OPENED)
		{
			client = (struct rt_net_client *)piple->client;
			if(client)
			{
				pthread_mutex_lock(&piple->send_lock);
				piple->send_pkt.head.cmd = CLOSE_PIPLE;
				piple->send_pkt.head.piple_id = piple->id;
				piple->send_pkt.head.data_len = 0;
				if(tcp_send_pkt(client, (uint8_t *)&piple->send_pkt, sizeof(piple->send_pkt.head)) != -1)
				{
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
				}else{
					func_error("tcp_send_pkt error.");
				}
				pthread_mutex_lock(&piple->send_lock);
			}else{
				func_error("not find client.");
			}
		}else{
			func_error("piple not open.");
		}
	}else{
		func_error("not find piple.");
	}
	return ret;
}

int rt_net_send(
	Piple piple_fd,
	const uint8_t *buf,
	size_t len)
{
	int ret = -1;
	struct rt_net_client *client;
	struct rt_net_piple *piple = (struct rt_net_piple *)piple_fd;
	if(piple)
	{
		if(piple->state == PIPLE_OPENED)
		{
			client = (struct rt_net_client *)piple->client;
			if(client)
			{
				pthread_mutex_lock(&piple->send_lock);
				piple->send_pkt.head.cmd = PIPLE_DATA;
				piple->send_pkt.head.piple_id = piple->id;
				memcpy(piple->send_pkt.data, buf, len);
				piple->send_pkt.head.data_len = len;
				ret = tcp_send_pkt(client, (const uint8_t *)&piple->send_pkt, sizeof(piple->send_pkt.head));
				pthread_mutex_lock(&piple->send_lock);
			}else{
				func_error("not find client.");
			}
		}else{
			func_error("piple not open.");
		}
	}else{
		func_error("not find piple.");
	}
	return ret;
}

void _on_data_recv(struct rt_net_client *client, uint8_t *buf, uint16_t len)
{
	struct rt_net_piple *piple;
	struct piple_pkt *pkt = (struct piple_pkt *)buf;

	if(len >= sizeof(pkt->head))
	{
		switch(pkt->head.cmd)
		{
			case OPEN_PIPLE:
			{
				piple = client->piples[pkt->head.piple_id];
				if(piple == NULL)
				{
					piple = calloc(1, sizeof(struct rt_net_piple));
					if(piple == NULL)
					{
						pkt->head.cmd = OPEN_REP_ERROR;
					}else{
						client->piples[pkt->head.piple_id] = piple;
						pkt->head.cmd = OPEN_REP_SUCCESS;
					}
				}else{
					pkt->head.cmd = OPEN_REP_SUCCESS;
				}
				if(pkt->head.cmd == OPEN_REP_SUCCESS)
				{
					piple->id = pkt->head.piple_id;
					piple->client = (Client)client;
					piple->state = PIPLE_OPENING;
					piple->piple_type = pkt->head.piple_type;
					pthread_mutex_init(&piple->send_lock, NULL);
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
							client->piples[client->piple_idx]->recv_callback((Piple)client->piples[client->piple_idx], pkt->data, pkt->head.data_len);
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
		func_error("pkt is too small, len:%d", len);
	}
}