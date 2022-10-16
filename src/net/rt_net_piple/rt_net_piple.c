#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "util.h"
#include "rt_net_piple.h"
#include "rt_net_socket.h"

#define MSG_TIMEOUT 10
pthread_mutex_t server_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

#define MAX_SERVER 64
uint16_t server_idx = 0;
struct rt_net_server *servers[MAX_SERVER];

static uint64_t tm_to_ns(struct timespec tm)
{
	return tm.tv_sec * 1000000000 + tm.tv_nsec;
}

static struct timespec ns_to_tm(uint64_t ns)
{
	struct timespec tm;
	tm.tv_sec = ns / 1000000000;
	tm.tv_nsec = ns - (tm.tv_sec * 1000000000);
	return tm;
}

static int _time_outwait(
	struct rt_net_client *client,
	pthread_cond_t *__restrict __cond,
	pthread_mutex_t *__restrict __mutex)
{
	struct timespec start_tm;
	struct timespec end_tm;
	clock_gettime(CLOCK_MONOTONIC, &start_tm);
	end_tm = ns_to_tm(tm_to_ns(start_tm) + client->msg_timeout * 1000000000UL);
	if(pthread_cond_timedwait(__cond, __mutex, &end_tm) == ETIMEDOUT)
		return -1;

	return 0;
}

struct rt_net_server *_get_server(uint32_t uri)
{
	struct rt_net_server *server = servers[URI_SERVER(uri)];
	if(!server)
	{
		func_error("not find server, server_id:%d", URI_SERVER(uri));
	}
	return server;
}

struct rt_net_client *_get_client(uint32_t uri)
{
	struct rt_net_client *client = NULL;
	struct rt_net_server *server = _get_server(uri);
	if(server)
	{
		client = server->clients[URI_CLIENT(uri)];
		if(!client)
		{
			func_error("not find client, id:%d", URI_CLIENT(uri));
		}
	}
	return client;
}

struct rt_net_piple *_get_piple(uint32_t uri)
{
	struct rt_net_piple *piple = NULL;
	struct rt_net_client *client = _get_client(uri);
	if(client)
	{
		piple = client->piples[URI_CHANNEL(uri)];
		if(!piple)
		{
			func_error("not find piple, id:%d", URI_CHANNEL(uri));
		}
	}
	return piple;
}

Server rt_net_init_server(char *addr, uint16_t port)
{
	Server ret = -1;
	struct rt_net_server *server;
	pthread_mutex_lock(&server_lock);

	if(server_idx < MAX_SERVER)
	{
		server = (struct rt_net_server *)calloc(1, sizeof(struct rt_net_server));
		if(server != NULL)
		{
			pthread_mutex_init(&server->accept_lock, NULL);
			if(net_server_init(server, addr, port) != -1)
			{
				servers[server_idx] = server;
				server->id = server_idx;
				server_idx++;
				ret = GET_URI(server->id, 0, 0);
			}else{
				free(server);
			}
		}else{
			func_error("calloc fail.");
		}
	}else{
		func_error("The number of server has reached the upper limit.");
	}

	pthread_mutex_unlock(&server_lock);

	return ret;
}

int rt_net_release_server(Server server_fd)
{
	int i;
	int ret = -1;
	struct rt_net_server *server = _get_server(server_fd);
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

void init_next_pkt(struct rt_net_client *client, struct piple_pkt *pkt)
{
	pkt->head.idx = client->send_pkt_idx;
	client->send_pkt_idx++;
}

int __send_cmd(
	struct rt_net_client *client,
	struct piple_pkt *pkt,
	pthread_mutex_t *lock,
	enum CMD_RESPONSE *cmd_ret,
	bool tcp_udp,
	enum CMD_RESPONSE success,
	enum CMD_RESPONSE fail)
{
	int ret = 1;
	pthread_mutex_lock(lock);

	if(tcp_udp)
	{
		if(tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head)) == -1)
		{
			func_error("tcp_send_pkt error.");
			ret = -1;
		}
	}else{
		if(udp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head)) == -1)
		{
			func_error("udp_send_pkt error.");
			ret = -1;
		}
		if(udp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head)) == -1)
		{
			func_error("udp_send_pkt error.");
			ret = -1;
		}
		if(udp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head)) == -1)
		{
			func_error("udp_send_pkt error.");
			ret = -1;
		}
	}

	while(ret == 1)
	{
		if(*cmd_ret == success)
		{
			ret = 0;
			*cmd_ret = CMD_NONE;
			break;
		}
		if(*cmd_ret == fail)
		{
			ret = -1;
			*cmd_ret = CMD_NONE;
			break;
		}
		if(_time_outwait(client, &client->msg_signal, lock) == -1)
		{
			ret = -1;
			log_warning("signal: waiting timeout.");
			break;
		}
	}
	pthread_mutex_unlock(lock);
	return ret;
}

int _client_send_cmd(
	struct rt_net_client *client,
	struct piple_pkt *pkt,
	bool tcp_udp,
	enum CMD_RESPONSE success,
	enum CMD_RESPONSE fail)
{
	return __send_cmd(client, pkt, &client->client_lock, &client->cmd_ret, tcp_udp, success, fail);
}

int _piple_send_cmd(
	struct rt_net_client *client,
	struct rt_net_piple *piple,
	struct piple_pkt *pkt,
	bool tcp_udp,
	enum CMD_RESPONSE success,
	enum CMD_RESPONSE fail)
{
	return __send_cmd(client, pkt, &piple->piple_lock, &piple->cmd_ret, tcp_udp, success, fail);
}

Client rt_net_init_client(char *addr, uint16_t port)
{
	Client ret = -1;
	struct piple_pkt pkt;
	struct rt_net_client *client;
	struct rt_net_server *server;

	pthread_mutex_lock(&server_lock);
	if(servers[MAX_SERVER] == NULL)
	{
		server = (struct rt_net_server *)calloc(1, sizeof(struct rt_net_server));
		if(server != NULL)
		{
			servers[MAX_SERVER] = server;
			server->id = MAX_SERVER;
		}else{
			func_error("calloc fail.");
		}
	}else{
		server = servers[MAX_SERVER];
	}
	pthread_mutex_unlock(&server_lock);
	if(!server)
		return -1;

	pthread_mutex_lock(&client_lock);
	client = (struct rt_net_client *)calloc(1, sizeof(struct rt_net_client));
	if(client != NULL)
	{
		pthread_mutex_init(&client->client_lock, NULL);

		pthread_condattr_t attr;
		pthread_condattr_init(&attr);
		pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
		pthread_cond_init(&client->msg_signal, &attr);

		server->clients[server->client_idx] = client;
		client->id = server->client_idx;
		client->server = server;
		client->msg_timeout = MSG_TIMEOUT;
		server->client_idx++;
		if(net_client_init(client, addr, port) != -1)
		{
			init_next_pkt(client, &pkt);
			pkt.head.cmd = TCP_CONNECT;
			pkt.head.uri = GET_URI(server->id, client->id, 0);
			pkt.head.piple_type = 0;
			pkt.head.data_len = 0;
			if(_client_send_cmd(client, &pkt, true, CMD_TCP_CONNECT_SUCCESS, CMD_TCP_CONNECT_ERROR) == 0)
			{
				ret = GET_URI(server->id, client->id, 0);
				init_next_pkt(client, &pkt);
				pkt.head.uri = client->uri;
				pkt.head.cmd = UDP_CONNECT;
				if(_client_send_cmd(client, &pkt, false, CMD_UDP_CONNECT_SUCCESS, CMD_UDP_CONNECT_ERROR) != 0)
				{
					func_error("udp connect fail.");
				}
			}else{
				func_error("tcp connect fail.");
			}
		}else{
			free(client);
		}
	}else{
		func_error("calloc fail.");
	}
	pthread_mutex_unlock(&client_lock);

	return ret;
}

int _release_client(struct rt_net_client *client)
{
	int ret = -1;
	struct rt_net_server *server = NULL;

	if(client != NULL)
	{
		pthread_mutex_lock(&client_lock);
		if(client->server)
		{
			pthread_mutex_lock(&server_lock);
			server = client->server;
			if(server->clients && server->clients[client->id])
			{
				server->clients[client->id] = NULL;
				pthread_mutex_lock(&client->client_lock);
				pthread_cond_broadcast(&client->msg_signal);
				pthread_mutex_unlock(&client->client_lock);
			}
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

int rt_net_release_client(Client client_fd)
{
	struct rt_net_client *client = _get_client(client_fd);

	if(client != NULL)
	{
		net_client_release(client);
	}else{
		func_error("Not find client.");
	}

	return 0;
}

Client rt_net_server_accept(Server server_fd)
{
	Client ret = -1;
	struct rt_net_client *client;
	struct rt_net_server *server = _get_server(server_fd);

	if(server)
	{
		pthread_mutex_lock(&server->accept_lock);
		client = (struct rt_net_client *)calloc(1, sizeof(struct rt_net_client));
		if(client != NULL)
		{
			pthread_mutex_init(&client->client_lock, NULL);
			pthread_mutex_init(&client->client_lock, NULL);
			pthread_condattr_t attr;
			pthread_condattr_init(&attr);
			pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
			pthread_cond_init(&client->msg_signal, &attr);
			if(tcp_accept(server, client) == 0)
			{
				server->clients[server->client_idx] = client;
				client->id = server->client_idx;
				client->server = server;
				client->msg_timeout = MSG_TIMEOUT;
				server->client_idx++;
				ret = GET_URI(server->id, client->id, 0);
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
	uint8_t piple_channel,
	void *(* recv_callback)(Piple piple_fd, uint8_t *buf, uint16_t len))
{
	Piple ret = -1;
	struct piple_pkt pkt;
	struct rt_net_piple *piple;
	struct rt_net_client *client = _get_client(client_fd);
	struct rt_net_server *server;

	if(client)
	{
		server = client->server;
		if(!server)
		{
			func_error("not find server.");
			return -1;
		}

		piple = calloc(1, sizeof(struct rt_net_piple));
		if(piple != NULL)
		{
			if(!client->piples[piple_channel])
			{
				client->piples[piple_channel] = piple;
				piple->uri = GET_URI(server->id, client->id, piple_channel);
				piple->opposite_uri = 0;
				piple->client = client;

				piple->piple_type = piple_type;
				piple->recv_callback = recv_callback;
				piple->state = PIPLE_OPENING;

				pthread_mutex_init(&piple->piple_lock, NULL);
				init_next_pkt(client, &pkt);
				pkt.head.cmd = OPEN_PIPLE;
				pkt.head.uri = piple->uri;
				pkt.head.piple_type = piple->piple_type;
				pkt.head.data_len = 0;
				if(_client_send_cmd(client, &pkt, true, CMD_OPEN_SUCCESS, CMD_OPEN_ERROR) == 0)
				{
					pthread_mutex_lock(&piple->piple_lock);
					while(1)
					{
						if(piple->state == PIPLE_OPENED)
						{
							ret = GET_URI(server->id, client->id, piple_channel);
							piple->cmd_ret = CMD_NONE;
							break;
						}
						if(_time_outwait(client, &client->msg_signal, &piple->piple_lock) == -1)
						{
							func_error("signal: waiting timeout.");
							break;
						}
					}
					pthread_mutex_unlock(&piple->piple_lock);
				}else{
					piple->state = PIPLE_ERROR;
					func_error("Piple open error.");
				}
			}else{
				func_error("The channel is already open.");
			}
		}else{
			func_error("calloc fail.");
		}
	}else{
		func_error("Not find client.");
	}
	return ret;
}

Piple rt_net_piple_accept(Client client_fd)
{
	int i;
	Piple ret = -2;
	int client_id;
	struct rt_net_server *server;
	struct rt_net_client *client = _get_client(client_fd);

	if(client)
	{
		client_id = client->id;
		server = client->server;
		if(!server)
		{
			func_error("not find server.");
			return -1;
		}

		pthread_mutex_lock(&client->client_lock);
		while(ret == -2)
		{
			if(server->clients[client_id] == NULL)
			{
				ret = -1;
				break;
			}
			for (i = 0; i < PIPLE_MAX; ++i)
			{
				if(client->piples[i] != NULL &&
					client->piples[i]->state == PIPLE_OPENING)
				{
					client->piples[i]->state = PIPLE_WAITING;
					client->piples[i]->uri = GET_URI(server->id, client->id, i);
					ret = client->piples[i]->uri;
					break;
				}
			}
			if(ret == -2)
				pthread_cond_wait(&client->msg_signal, &client->client_lock);
		}
		pthread_mutex_unlock(&client->client_lock);
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
	struct rt_net_piple *piple = _get_piple(piple_fd);

	if(piple)
	{
		client = piple->client;
		if(client)
		{
			piple->recv_callback = recv_callback;

			init_next_pkt(client, &pkt);
			pkt.head.cmd = BIND_PIPLE;
			pkt.head.uri = piple->opposite_uri;
			pkt.head.data_len = 0;
			if(_piple_send_cmd(client, piple, &pkt, true, CMD_BIND_SUCCESS, CMD_BIND_ERROR) == 0)
			{
				ret = 0;
			}else{
				func_error("bind piple response error.");
			}
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
	uint8_t piple_channel)
{
	struct rt_net_client *client = _get_client(client_fd);
	if(client)
	{
		if(client->piples[piple_channel] != NULL && client->piples[piple_channel]->state == PIPLE_OPENED)
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
	uint8_t piple_channel)
{
	struct rt_net_server *server;
	struct rt_net_client *client = _get_client(client_fd);
	if(client)
	{
		server = client->server;
		if(!server)
		{
			func_error("not find server.");
			return -1;
		}

		if(client->piples[piple_channel] != NULL && client->piples[piple_channel]->state == PIPLE_OPENED)
		{
			return GET_URI(server->id, client->id, piple_channel);
		}
	}else{
		func_error("not find client.");
	}
	return -1;
}

int rt_net_close_piple(Piple piple_fd)
{
	int ret = -1;
	struct rt_net_client *client;
	struct rt_net_piple *piple = _get_piple(piple_fd);

	if(piple)
	{
		if(piple->state == PIPLE_OPENED)
		{
			client = piple->client;
			if(client)
			{
				init_next_pkt(client, &piple->send_pkt);
				piple->send_pkt.head.cmd = CLOSE_PIPLE;
				piple->send_pkt.head.uri = piple->opposite_uri;
				piple->send_pkt.head.data_len = 0;
				if(_client_send_cmd(client, &piple->send_pkt, true, CMD_CLOSE_PIPLE_SUCCESS, CMD_CLOSE_PIPLE_ERROR) == 0)
				{
					client->piples[URI_CHANNEL(piple->uri)] = NULL;
					free(piple);
					ret = 0;
				}else{
					func_error("bind piple response error.");
				}
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

int _send(
	struct rt_net_client *client,
	struct rt_net_piple *piple,
	const uint8_t *buf,
	size_t len)
{
	init_next_pkt(client, &piple->send_pkt);
	piple->send_pkt.head.cmd = PIPLE_DATA;
	piple->send_pkt.head.uri = piple->opposite_uri;
	memcpy(piple->send_pkt.data, buf, len);
	piple->send_pkt.head.data_len = len;
	switch(piple->piple_type)
	{
		case RELIABLE_PIPLE:
		{
			if(tcp_send_pkt(client, (const uint8_t *)&piple->send_pkt, len + sizeof(piple->send_pkt.head)) > 0)
				return len;
			break;
		}
		case RT_IMPORTANT_PIPLE:
		{
			for (int i = 0; i < 3; ++i)
			{
				if(udp_send_pkt(client, (const uint8_t *)&piple->send_pkt, len + sizeof(piple->send_pkt.head)) == -1)
					return -1;
			}
			return len;
			break;
		}
		case RT_UNIMPORTANT_PIPLE:
		{
			break;
		}
		default:
		{
			log_error("unkonw piple type:%d.", piple->piple_type);
			return -1;
		}
	}

	return -1;
}

int rt_net_send(
	Piple piple_fd,
	const uint8_t *buf,
	size_t len)
{
	int ret = -1;
	int send_tmp;
	struct rt_net_client *client;
	struct rt_net_piple *piple = _get_piple(piple_fd);

	if(piple)
	{
		if(piple->state == PIPLE_OPENED)
		{
			client = piple->client;
			if(client)
			{
				pthread_mutex_lock(&piple->piple_lock);
				ret = 0;
				while(len > 0)
				{
					if(len > PIPLE_DATA_LEN_MAX)
					{
						send_tmp = PIPLE_DATA_LEN_MAX;
					}else{
						send_tmp = len;
					}
					send_tmp = _send(client, piple, buf + ret, send_tmp);
					if(send_tmp == -1)
					{
						func_error("send error\n");
						ret = -1;
						break;
					}
					ret += send_tmp;
					len -= send_tmp;
				}
				pthread_mutex_unlock(&piple->piple_lock);
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