#include <errno.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>

#include "util.h"
#include "rt_net_piple.h"
#include "rt_net_socket.h"

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
		case TCP_CONNECT_REP_ERROR:
		{
			cmd = "TCP_CONNECT_REP_ERROR";
			break;
		}
		case TCP_CONNECT_REP_SUCCESS:
		{
			cmd = "TCP_CONNECT_REP_SUCCESS";
			break;
		}
		case UDP_CONNECT:
		{
			cmd = "UDP_CONNECT";
			break;
		}
		case UDP_CONNECT_REP_ERROR:
		{
			cmd = "UDP_CONNECT_REP_ERROR";
			break;
		}
		case UDP_CONNECT_REP_SUCCESS:
		{
			cmd = "UDP_CONNECT_REP_SUCCESS";
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
	log_info("cmd:%25s %s client:%p piple:%d.", cmd, direction_str, client, URI_CHANNEL(pkt->head.uri));
}

void sync_cmd_ret(struct rt_net_client *client, enum CMD_RESPONSE *cmd_ret, pthread_mutex_t *lock, enum CMD_RESPONSE ret)
{
	pthread_mutex_lock(lock);
	*cmd_ret = ret;
	pthread_cond_broadcast(&client->msg_signal);
	pthread_mutex_unlock(lock);
}

void _on_data_recv(struct rt_net_client *client, uint8_t *buf, uint16_t len, struct rt_net_client *udp_client)
{
	struct rt_net_piple *piple;
	struct rt_net_server *server;
	struct piple_pkt *pkt = (struct piple_pkt *)buf;

	if(len >= sizeof(pkt->head))
	{
		if(client->recv_pkt_idx <= pkt->head.idx)
		{
			client->recv_pkt_idx = pkt->head.idx + 1;
			init_next_pkt(client, pkt);
			_show_protocol(client, buf, len, false);
		}else{
			// printf("drop:\n");
			return;
		}
		
		if(pkt->head.cmd > OPEN_PIPLE)
		{
			piple = client->piples[URI_CHANNEL(pkt->head.uri)];
			if(!piple)
			{
				func_error("not find piple, piple id:%x channel:%d", pkt->head.uri, URI_CHANNEL(pkt->head.uri));
			}
		}

		switch(pkt->head.cmd)
		{
			case TCP_CONNECT:
			{
				server = client->server;
				if(server)
				{
					client->uri = pkt->head.uri;
					pkt->head.uri = GET_URI(server->id, client->id, 0);
					pkt->head.cmd = TCP_CONNECT_REP_SUCCESS;
				}else{
					pkt->head.cmd = TCP_CONNECT_REP_ERROR;
					func_error("server not find.");
				}
				tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case TCP_CONNECT_REP_ERROR:
			{
				sync_cmd_ret(client, &client->cmd_ret, &client->client_lock ,CMD_TCP_CONNECT_ERROR);
				break;
			}
			case TCP_CONNECT_REP_SUCCESS:
			{
				client->uri = pkt->head.uri;
				sync_cmd_ret(client, &client->cmd_ret, &client->client_lock ,CMD_TCP_CONNECT_SUCCESS);
				break;
			}
			case UDP_CONNECT:
			{
				client->udp_state = true;
				client->udp_fd = udp_client->udp_fd;
				memcpy(&client->udp_opposite_addr, &udp_client->udp_opposite_addr, sizeof(client->udp_opposite_addr));
				pkt->head.uri = client->uri;
				pkt->head.cmd = UDP_CONNECT_REP_SUCCESS;
				udp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));
				break;
			}
			case UDP_CONNECT_REP_ERROR:
			{
				sync_cmd_ret(client, &client->cmd_ret, &client->client_lock ,CMD_UDP_CONNECT_ERROR);
				break;
			}
			case UDP_CONNECT_REP_SUCCESS:
			{
				sync_cmd_ret(client, &client->cmd_ret, &client->client_lock ,CMD_UDP_CONNECT_SUCCESS);
				break;
			}
			case OPEN_PIPLE:
			{
				server = client->server;
				if(server)
				{
					piple = calloc(1, sizeof(struct rt_net_piple));
					if(piple == NULL)
					{
						func_error("calloc fail.");
						pkt->head.cmd = OPEN_REP_ERROR;
					}else{
						client->piples[URI_CHANNEL(pkt->head.uri)] = piple;
						piple->opposite_uri = pkt->head.uri;
						pkt->head.cmd = OPEN_REP_SUCCESS;
					}
				}else{
					pkt->head.cmd = OPEN_REP_ERROR;
					func_error("server not find.");
				}

				pkt->head.uri = GET_URI(server->id, client->id, URI_CHANNEL(pkt->head.uri));
				tcp_send_pkt(client, (uint8_t *)pkt, sizeof(pkt->head));

				if(pkt->head.cmd == OPEN_REP_SUCCESS)
				{
					pthread_mutex_lock(&client->client_lock);
					piple->client = client;
					piple->state = PIPLE_OPENING;
					piple->piple_type = pkt->head.piple_type;
					pthread_mutex_init(&piple->piple_lock, NULL);
					pthread_cond_broadcast(&client->msg_signal);
					pthread_mutex_unlock(&client->client_lock);
				}

				break;
			}
			case OPEN_REP_ERROR:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_ERROR;
					sync_cmd_ret(client, &client->cmd_ret, &client->client_lock ,CMD_OPEN_ERROR);
				}
				break;
			}
			case OPEN_REP_SUCCESS:
			{
				if(piple != NULL)
				{
					piple->opposite_uri = pkt->head.uri;
					sync_cmd_ret(client, &client->cmd_ret, &client->client_lock ,CMD_OPEN_SUCCESS);
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
					sync_cmd_ret(client, &piple->cmd_ret, &piple->piple_lock ,CMD_BIND);
				}
				break;
			}
			case BIND_REP_ERROR:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_ERROR;
					sync_cmd_ret(client, &piple->cmd_ret, &piple->piple_lock ,CMD_BIND_ERROR);
				}
				break;
			}
			case BIND_REP_SUCCESS:
			{
				if(piple != NULL)
				{
					printf("BIND_REP_SUCCESS0\n");
					piple->state = PIPLE_OPENED;
					sync_cmd_ret(client, &piple->cmd_ret, &piple->piple_lock ,CMD_BIND_SUCCESS);
					printf("BIND_REP_SUCCESS1\n");
				}
					printf("BIND_REP_SUCCESS2\n");
				break;
			}
			case PIPLE_DATA:
			{
				if(piple != NULL)
				{
					if(piple->state == PIPLE_OPENED)
					{
						if(piple->recv_callback)
							piple->recv_callback(piple->uri, pkt->data, pkt->head.data_len);
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
					client->piples[URI_CHANNEL(piple->opposite_uri)] = NULL;
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
					sync_cmd_ret(client, &client->cmd_ret, &client->client_lock ,CMD_CLOSE_PIPLE_ERROR);
				}
				break;
			}
			case CLOSE_PIPLE_REP_SUCCESS:
			{
				if(piple != NULL)
				{
					piple->state = PIPLE_CLOSED;
					sync_cmd_ret(client, &client->cmd_ret, &client->client_lock ,CMD_CLOSE_PIPLE_SUCCESS);
				}
				break;
			}
		}
	}else{
		func_error("pkt is too small, len:%d", len);
	}
}

void _on_udp_data_recv(struct rt_net_client *udp_client, uint8_t *buf, uint16_t len)
{
	struct rt_net_client *client;
	struct piple_pkt *pkt = (struct piple_pkt *)buf;

	if(len >= sizeof(pkt->head))
	{
		client = _get_client(pkt->head.uri);
		if(client)
		{
			_on_data_recv(client, buf, len, udp_client);
		}
	}else{
		func_error("pkt is too small, len:%d", len);
	}
}