#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "rt_net_piple.h"

void *callback(int piple_id, uint8_t *buf, uint16_t len)
{
	printf("piple_id:%d len:%u buf:%s\n", piple_id, len, buf);
	return NULL;
}

int main()
{
	int client_id;
	int piple_id;

	client_id = rt_net_init_client("127.0.0.1", 6000);
	if(client_id == -1)
	{
		printf("init client fail.\n");
		return -1;
	}

	piple_id = rt_net_open_piple(NONE_SERVER, client_id, RELIABLE_PIPLE, 0, callback);
	if(piple_id == -1)
	{
		printf("open piple fail.\n");
		return -1;
	}

	rt_net_send(NONE_SERVER, client_id, piple_id, (const uint8_t *)"123", 4);
	sleep(3);
	return 0;
}