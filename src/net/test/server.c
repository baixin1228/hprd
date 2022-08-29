#include <stdio.h>
#include <stdlib.h>
#include "rt_net_piple.h"

void *callback(Piple piple_fd, uint8_t *buf, uint16_t len)
{
	printf("piple_fd:%p len:%u buf:%s\n", piple_fd, len, buf);
	rt_net_send(piple_fd, (const uint8_t *)"return", 7);
	return NULL;
}

int main()
{
	Server server_fd;
	Client client_fd;
	Piple piple_fd;
	server_fd = rt_net_init_server("0.0.0.0", 6000);
	if(server_fd == NULL)
	{
		printf("init server fail.\n");
		return -1;
	}

	while(1)
	{
		client_fd = rt_net_server_accept(server_fd);
		if(client_fd == NULL)
		{
			printf("server_accept fail client_fd is null.\n");
			break;
		}
		while(1)
		{
			piple_fd = rt_net_piple_accept(client_fd);
			if(piple_fd == NULL)
			{
				printf("piple_accept fail piple_fd is null.\n");
				break;
			}
			rt_net_piple_bind(piple_fd, callback);
		}
	}
	return 0;
}