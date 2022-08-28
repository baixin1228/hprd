#ifndef __RT_NET_PIPLE_H__
#define __RT_NET_PIPLE_H__
#include <stdint.h>
#include <stdbool.h>
#include "rt_net_socket.h"

#define NONE_SERVER -1

int rt_net_init_server(char * addr, uint16_t port);
int rt_net_release_server(int server_id);

int rt_net_init_client(char * addr, uint16_t port);
int rt_net_release_client(int server_id, int client_id);

int rt_net_server_accept(int server_id);

int rt_net_open_piple(
	int server_id,
	int client_id,
	enum RT_PIPLE_TYPE piple_type,
	uint8_t piple_id,
	void *(* callback)(int piple_id, uint8_t *buf, uint16_t len));

int rt_net_piple_accept(
	int server_id,
	int client_id);

int rt_net_piple_bind(
	int server_id,
	int client_id,
	uint8_t piple_id,
	void *(* recv_callback)(int piple_id, uint8_t *buf, uint16_t len));

bool rt_net_has_piple(
	int server_id,
	int client_id,
	uint8_t piple_id);

int rt_net_close_piple(
	int server_id,
	int client_id,
	int piple_id);

int rt_net_send(
	int server_id,
	int client_id,
	int piple_id,
	const uint8_t *buf,
	size_t len);

void _on_data_recv(struct rt_net_client *client, uint8_t *buf, uint16_t len);
#endif