#include <errno.h>
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

Server rt_net_init_server(char *addr, uint16_t port)
{
	Server ret = NULL;
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
				ret = (Server)server;
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
	struct rt_net_server *server = (struct rt_net_server *)server_fd;
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

enum CMD_RESPONSE __send_cmd(struct rt_net_client *client, struct piple_pkt *pkt, pthread_mutex_t *lock, enum CMD_RESPONSE *cmd_ret, bool tcp_udp)
{
	enum CMD_RESPONSE ret = CMD_ERROR;
	pthread_mutex_lock(lock);
	*cmd_ret = CMD_NONE;

	if(tcp_udp)
	{
		if(tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head)) != -1)
		{
			func_error("tcp_send_pkt error.");
			return ret;
		}
	}else{
		if(udp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head)) != -1)
		{
			func_error("udp_send_pkt error.");
			return ret;
		}
		if(udp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head)) != -1)
		{
			func_error("udp_send_pkt error.");
			return ret;
		}
		if(udp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head)) != -1)
		{
			func_error("udp_send_pkt error.");
			return ret;
		}
	}

	while(1)
	{
		if(*cmd_ret != CMD_NONE)
		{
			ret = *cmd_ret;
			break;
		}
		if(_time_outwait(client, &client->msg_signal, lock) == -1)
		{
			ret = CMD_TIME_OUT;
			log_warning("signal: waiting timeout.");
			break;
		}
	}
	pthread_mutex_unlock(lock);
	return ret;
}

enum CMD_RESPONSE _client_send_cmd(struct rt_net_client *client, struct piple_pkt *pkt, bool tcp_udp)
{
	return __send_cmd(client, pkt, &client->client_lock, &client->cmd_ret, tcp_udp);
}

enum CMD_RESPONSE _piple_send_cmd(struct rt_net_client *client, struct rt_net_piple *piple, struct piple_pkt *pkt, bool tcp_udp)
{
	return __send_cmd(client, pkt, &piple->piple_lock, &piple->cmd_ret, tcp_udp);
}

Client rt_net_init_client(char *addr, uint16_t port)
{
	Client ret = NULL;
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
		return NULL;

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
			if(_client_send_cmd(client, &pkt, true) == CMD_OK)
			{
				ret = (Client)client;
				init_next_pkt(client, &pkt);
				pkt.head.cmd = UDP_CONNECT;
				if(_client_send_cmd(client, &pkt, false) != CMD_OK)
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

int rt_net_release_client(Client client_fd)
{
	int ret = -1;
	struct rt_net_server *server = NULL;
	struct rt_net_client *client = (struct rt_net_client *)client_fd;

	if(client != NULL)
	{
		pthread_mutex_lock(&client_lock);
		if(client->server)
		{
			pthread_mutex_lock(&server_lock);
			server = (struct rt_net_server *)client->server;
			if(server->clients && server->clients[client->id])
				server->clients[client->id] = NULL;
			pthread_mutex_unlock(&server_lock);
		}
		net_client_release(client);
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
				client->server = server_fd;
				client->msg_timeout = MSG_TIMEOUT;
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
	uint8_t piple_channel,
	void *(* recv_callback)(Piple piple_fd, uint8_t *buf, uint16_t len))
{
	Piple ret = NULL;
	struct piple_pkt pkt;
	struct rt_net_piple *piple;
	struct rt_net_client *client = (struct rt_net_client *)client_fd;
	struct rt_net_server *server;

	if(client)
	{
		server = (struct rt_net_server *)client->server;
		if(!server)
		{
			func_error("not find server.");
			return NULL;
		}

		piple = calloc(1, sizeof(struct rt_net_piple));
		if(piple != NULL)
		{
			if(!client->piples[piple_channel])
			{
				client->piples[piple_channel] = piple;
				piple->uri = 0;
				piple->client = (Client)client;

				piple->piple_type = piple_type;
				piple->recv_callback = recv_callback;
				piple->state = PIPLE_OPENING;

				pthread_mutex_init(&piple->piple_lock, NULL);
				init_next_pkt(client, &pkt);
				pkt.head.cmd = OPEN_PIPLE;
				pkt.head.uri = GET_URI(server->id, client->id, piple_channel);
				pkt.head.piple_type = piple->piple_type;
				pkt.head.data_len = 0;
				if(_client_send_cmd(client, &pkt, true) == CMD_OK)
				{
					ret = (Piple)piple;
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
	Piple ret = NULL;
	struct rt_net_client *client = (struct rt_net_client *)client_fd;

	if(client)
	{
		pthread_mutex_lock(&client->client_lock);
		while(!ret)
		{
			for (i = 0; i < PIPLE_MAX; ++i)
			{
				if(client->piples[i] != NULL &&
					client->piples[i]->state == PIPLE_OPENING)
				{
					ret = (Piple)client->piples[i];
					break;
				}
			}
			if(!ret)
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
	struct rt_net_piple *piple = (struct rt_net_piple *)piple_fd;

	printf("%s\n", __func__);
	if(piple)
	{
		client = (struct rt_net_client *)piple->client;
		if(client)
		{
			piple->recv_callback = recv_callback;

			init_next_pkt(client, &pkt);
			pkt.head.cmd = BIND_PIPLE;
			pkt.head.uri = piple->uri;
			pkt.head.data_len = 0;
			if(_piple_send_cmd(client, piple, &pkt, true) == CMD_OK)
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
	struct rt_net_client *client = (struct rt_net_client *)client_fd;
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
	struct rt_net_client *client = (struct rt_net_client *)client_fd;
	if(client)
	{
		if(client->piples[piple_channel] != NULL && client->piples[piple_channel]->state == PIPLE_OPENED)
		{
			return (Piple)client->piples[piple_channel];
		}
	}else{
		func_error("not find client.");
	}
	return NULL;
}

int rt_net_close_piple(Piple piple_fd)
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
				init_next_pkt(client, &piple->send_pkt);
				piple->send_pkt.head.cmd = CLOSE_PIPLE;
				piple->send_pkt.head.uri = piple->uri;
				piple->send_pkt.head.data_len = 0;
				if(_piple_send_cmd(client, piple, &piple->send_pkt, true) == CMD_OK)
				{
					client->piples[PIPLE_CHANNEL(piple->uri)] = NULL;
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
	piple->send_pkt.head.uri = piple->uri;
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
	struct rt_net_piple *piple = (struct rt_net_piple *)piple_fd;

	if(piple)
	{
		if(piple->state == PIPLE_OPENED)
		{
			client = (struct rt_net_client *)piple->client;
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
						printf("send error\n");
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

void _show_protocol(struct rt_net_client *client, const uint8_t *buf, uint16_t len, bool is_send)
{
	char *cmd;
	char *direction_str;
	struct piple_pkt *pkt = (struct piple_pkt *)buf;

	if(is_send)
	{
		direction_str = "---->";
	}else{
		direction_str = "<----";
	}

	switch(pkt->head.cmd)
	{
		case TCP_CONNECT:
		{
			cmd = "TCP_CONNECT";
			break;
		}
		case TCP_CONNECT_RET_ERROR:
		{
			cmd = "TCP_CONNECT_RET_ERROR";
			break;
		}
		case TCP_CONNECT_RET_SUCCESS:
		{
			cmd = "TCP_CONNECT_RET_SUCCESS";
			break;
		}
		case UDP_CONNECT:
		{
			cmd = "UDP_CONNECT";
			break;
		}
		case UDP_CONNECT_RET_ERROR:
		{
			cmd = "UDP_CONNECT_RET_ERROR";
			break;
		}
		case UDP_CONNECT_RET_SUCCESS:
		{
			cmd = "UDP_CONNECT_RET_SUCCESS";
			break;
		}
		case OPEN_PIPLE:
		{
			cmd = "OPEN_PIPLE";
			break;
		}
		case OPEN_REP_ERROR:
		{
			cmd = "OPEN_REP_ERROR";
			break;
		}
		case OPEN_REP_SUCCESS:
		{
			cmd = "OPEN_REP_SUCCESS";
			break;
		}
		case BIND_PIPLE:
		{
			cmd = "BIND_PIPLE";
			break;
		}
		case BIND_REP_ERROR:
		{
			cmd = "BIND_REP_ERROR";
			break;
		}
		case BIND_REP_SUCCESS:
		{
			cmd = "BIND_REP_SUCCESS";
			break;
		}
		case PIPLE_DATA:
		{
			cmd = "PIPLE_DATA";
			break;
		}
		case CLOSE_PIPLE:
		{
			cmd = "CLOSE_PIPLE";
			break;
		}
		case CLOSE_PIPLE_REP_ERROR:
		{
			cmd = "CLOSE_PIPLE_REP_ERROR";
			break;
		}
		case CLOSE_PIPLE_REP_SUCCESS:
		{
			cmd = "CLOSE_PIPLE_REP_SUCCESS";
			break;
		}
	}
	log_info("cmd:%25s %s client:%p piple:%d.", cmd, direction_str, client, PIPLE_CHANNEL(pkt->head.uri));
}

void sync_cmd_ret(struct rt_net_client *client, enum CMD_RESPONSE *cmd_ret, enum CMD_RESPONSE ret)
{
	pthread_mutex_lock(&client->client_lock);
	*cmd_ret = ret;
	pthread_cond_broadcast(&client->msg_signal);
	pthread_mutex_unlock(&client->client_lock);
}

void _on_data_recv(struct rt_net_client *client, uint8_t *buf, uint16_t len)
{
	struct rt_net_piple *piple;
	struct rt_net_server *server;
	struct piple_pkt *pkt = (struct piple_pkt *)buf;

	if(len >= sizeof(pkt->head))
	{
		_show_protocol(client, buf, len, false);
		
		if(pkt->head.cmd > OPEN_PIPLE)
		{
			piple = client->piples[PIPLE_CHANNEL(pkt->head.uri)];
			if(!piple)
			{
				func_error("not find piple, piple id:%x channel:%d", pkt->head.uri, PIPLE_CHANNEL(pkt->head.uri));
			}
		}

		switch(pkt->head.cmd)
		{
			case TCP_CONNECT:
			{
				client->uri = pkt->head.uri;
				pkt->head.cmd = TCP_CONNECT_RET_SUCCESS;
				tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case TCP_CONNECT_RET_ERROR:
			{
				sync_cmd_ret(client, &client->cmd_ret, CMD_ERROR);
				break;
			}
			case TCP_CONNECT_RET_SUCCESS:
			{
				sync_cmd_ret(client, &client->cmd_ret, CMD_OK);
				break;
			}
			case UDP_CONNECT:
			{
				client->udp_state = true;
				pkt->head.cmd = UDP_CONNECT_RET_SUCCESS;
				udp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case UDP_CONNECT_RET_ERROR:
			{
				sync_cmd_ret(client, &client->cmd_ret, CMD_ERROR);
				break;
			}
			case UDP_CONNECT_RET_SUCCESS:
			{
				sync_cmd_ret(client, &client->cmd_ret, CMD_OK);
				break;
			}
			case OPEN_PIPLE:
			{
				server = (struct rt_net_server *)client->server;
				if(server)
				{
					piple = calloc(1, sizeof(struct rt_net_piple));
					if(piple == NULL)
					{
						func_error("calloc fail.");
						pkt->head.cmd = OPEN_REP_ERROR;
					}else{
						client->piples[PIPLE_CHANNEL(pkt->head.uri)] = piple;
						pkt->head.cmd = OPEN_REP_SUCCESS;
					}
				}else{
					pkt->head.cmd = OPEN_REP_ERROR;
					func_error("server not find.");
				}

				if(pkt->head.cmd == OPEN_REP_SUCCESS)
				{
					pthread_mutex_lock(&client->client_lock);
					piple->uri = pkt->head.uri;
					pkt->head.uri = GET_URI(server->id, client->id, PIPLE_CHANNEL(pkt->head.uri));
					piple->client = (Client)client;
					piple->state = PIPLE_OPENING;
					piple->piple_type = pkt->head.piple_type;
					pthread_mutex_init(&piple->piple_lock, NULL);
					pthread_cond_broadcast(&client->msg_signal);
					pthread_mutex_unlock(&client->client_lock);
				}
				tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case OPEN_REP_ERROR:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_ERROR;
					sync_cmd_ret(client, &client->cmd_ret, CMD_ERROR);
				}
				break;
			}
			case OPEN_REP_SUCCESS:
			{
				if(piple != NULL)
				{
					piple->uri = pkt->head.uri;
					sync_cmd_ret(client, &client->cmd_ret, CMD_OK);
				}
				break;
			}
			case BIND_PIPLE:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_OPENED;
					pkt->head.cmd = BIND_REP_SUCCESS;
					tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				}
				break;
			}
			case BIND_REP_ERROR:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_ERROR;
					sync_cmd_ret(client, &piple->cmd_ret, CMD_ERROR);
				}
				break;
			}
			case BIND_REP_SUCCESS:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_OPENED;
					sync_cmd_ret(client, &piple->cmd_ret, CMD_OK);
				}
				break;
			}
			case PIPLE_DATA:
			{
				if(piple != NULL)
				{
					if(piple->state == PIPLE_OPENED)
					{
						if(piple->recv_callback)
							piple->recv_callback((Piple)piple, pkt->data, pkt->head.data_len);
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
				if(piple == NULL)
				{
					pkt->head.cmd = CLOSE_PIPLE_REP_ERROR;
				}else{
					piple->state = PIPLE_CLOSED;
					client->piples[PIPLE_CHANNEL(piple->uri)] = NULL;
					free(piple);
					pkt->head.cmd = CLOSE_PIPLE_REP_SUCCESS;
				}
				tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case CLOSE_PIPLE_REP_ERROR:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_ERROR;
					sync_cmd_ret(client, &piple->cmd_ret, CMD_ERROR);
				}
				break;
			}
			case CLOSE_PIPLE_REP_SUCCESS:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_CLOSED;
					sync_cmd_ret(client, &piple->cmd_ret, CMD_OK);
				}
				break;
			}
		}
	}else{
		func_error("pkt is too small, len:%d", len);
	}
}

void _on_udp_data_recv(struct rt_net_client *client, uint8_t *buf, uint16_t len)
{
	struct rt_net_server *server;
	struct piple_pkt *pkt = (struct piple_pkt *)buf;

	if(len >= sizeof(pkt->head))
	{
		server = servers[PIPLE_SERVER(pkt->head.uri)];
		if(!server)
		{
			func_error("not find server, id:%d", PIPLE_SERVER(pkt->head.uri));
			return;
		}
		client = server->clients[PIPLE_CLIENT(pkt->head.uri)];
		printf("client:%p client:%d\n", client, PIPLE_CLIENT(pkt->head.uri));
		if(!client)
		{
			func_error("not find client, id:%d", PIPLE_CLIENT(pkt->head.uri));
			return;
		}

		_on_data_recv(client, buf, len);
	}else{
		func_error("pkt is too small, len:%d", len);
	}
}