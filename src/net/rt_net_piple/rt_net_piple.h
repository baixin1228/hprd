#ifndef __RT_NET_PIPLE_H__
#define __RT_NET_PIPLE_H__
#include <stdint.h>
#include <stdbool.h>
#include "rt_net_socket.h"

#define NONE_SERVER -1

Server rt_net_init_server(char *addr, uint16_t port);
int rt_net_release_server(Server server_fd);

Client rt_net_init_client(char *addr, uint16_t port);
int rt_net_release_client(Client client_fd);

Client rt_net_server_accept(Server server_fd);

Piple rt_net_open_piple(
	Client client_fd,
	enum RT_PIPLE_TYPE piple_type,
	uint8_t piple_id,
	void *(* recv_callback)(Piple piple_fd, uint8_t *buf, uint16_t len));

Piple rt_net_piple_accept(Client client_fd);

int rt_net_piple_bind(
	Piple piple_fd,
	void *(* recv_callback)(Piple piple_fd, uint8_t *buf, uint16_t len));

bool rt_net_has_piple(
	Client client_fd,
	uint8_t piple_id);

Piple rt_net_get_piple(
	Client client_fd,
	uint8_t piple_id);

int rt_net_close_piple(Piple piple_fd);

int rt_net_send(
	Piple piple_fd,
	const uint8_t *buf,
	size_t len);

void _on_data_recv(struct rt_net_client *client, uint8_t *buf, uint16_t len);
#endif