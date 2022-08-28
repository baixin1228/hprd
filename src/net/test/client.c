#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "rt_net_piple.h"

void *callback(Piple piple_fd, uint8_t *buf, uint16_t len)
{
	printf("piple_fd:%p len:%u buf:%s\n", piple_fd, len, buf);
	return NULL;
}

int main()
{
	Client client_fd;
	Piple piple_fd;

	client_fd = rt_net_init_client("127.0.0.1", 6000);
	if(client_fd == NULL)
	{
		printf("init client fail.\n");
		return -1;
	}

	piple_fd = rt_net_open_piple(client_fd, RELIABLE_PIPLE, 0, callback);
	if(piple_fd == NULL)
	{
		printf("open piple fail.\n");
		return -1;
	}

	rt_net_send(piple_fd, (const uint8_t *)"123", 4);
	sleep(3);
	return 0;
}