#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "rt_net_piple.h"

void *callback(Piple piple_fd, uint8_t *buf, uint16_t len)
{
	printf("piple_fd:%ld len:%u buf:%s\n", piple_fd, len, buf);
	rt_net_send(piple_fd, (const uint8_t *)"return", 7);
	return NULL;
}

void *piple_accept(void *arg)
{
	Piple piple_fd;
	Client client_fd = (Client)arg;
	printf("piple_accept thread start.\n");
	while(1)
	{
		piple_fd = rt_net_piple_accept(client_fd);
		if(piple_fd == -1)
		{
			printf("piple_accept fail piple_fd is -1.\n");
			break;
		}
		rt_net_piple_bind(piple_fd, callback);
	}
	printf("piple_accept thread close.\n");
	return NULL;
}

int main()
{
	Server server_fd;
	Client client_fd;
	server_fd = rt_net_init_server("0.0.0.0", 6000);
	if(server_fd == -1)
	{
		printf("init server fail.\n");
		return -1;
	}

	pthread_t p;
	while(1)
	{
		client_fd = rt_net_server_accept(server_fd);
		if(client_fd == -1)
		{
			printf("server_accept fail client_fd is null.\n");
			break;
		}
		pthread_create(&p, NULL, piple_accept, (void *)client_fd);
	}
	return 0;
}