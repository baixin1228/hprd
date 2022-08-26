#ifndef __RT_NET_PIPLE_H__
#define __RT_NET_PIPLE_H__
#include <stdint.h>
#include <stdbool.h>

enum RT_PIPLE_TYPE
{
	RELIABLE_PIPLE,
	RT_IMPORTANT_PIPLE,
	RT_UNIMPORTANT_PIPLE,
};

struct rt_piple
{

};

struct rt_net_client
{

};

struct rt_net_server
{

};

int rt_net_init_server(struct rt_net_server *server, char * addr, uint16_t port);
int rt_net_release_server(struct rt_net_server *server);

int rt_net_init_client(struct rt_net_client *client, char * addr, uint16_t port);
int rt_net_release_client(struct rt_net_server *server);

int rt_net_server_accept(struct rt_net_server *server, struct rt_net_client *client);

int rt_net_open_piple(struct rt_net_client *client, struct rt_piple *piple, enum RT_PIPLE_TYPE piple_type, uint8_t piple_id);
int rt_net_get_piple(struct rt_net_client *client, struct rt_piple *piple, enum RT_PIPLE_TYPE piple_type, uint8_t piple_id);
int rt_net_close_piple(struct rt_net_client *client, struct rt_piple *piple);

int rt_net_send(struct rt_piple *piple, uint8_t buf, size_t len);
int rt_net_recv(struct rt_piple *piple, uint8_t buf, size_t len);
int rt_net_recv_block(struct rt_piple *piple, uint8_t buf, size_t len);
#endif