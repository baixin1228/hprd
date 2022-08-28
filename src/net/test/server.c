#include <stdio.h>
#include <stdlib.h>
#include "rt_net_piple.h"

int server_id;
int client_id;
void *callback(int piple_id, uint8_t *buf, uint16_t len)
{
	printf("piple_id:%d len:%u buf:%s\n", piple_id, len, buf);
	rt_net_send(server_id, client_id, piple_id, (const uint8_t *)"return", 7);
	return NULL;
}

int main()
{
	int piple_id;
	server_id = rt_net_init_server("0.0.0.0", 6000);
	if(server_id == -1)
	{
		printf("init server fail.\n");
		return -1;
	}

	while(1)
	{
		client_id = rt_net_server_accept(server_id);
		if(client_id == -1)
		{
			break;
		}
		piple_id = rt_net_piple_accept(server_id, client_id);
		if(piple_id == -1)
		{
			break;
		}
		rt_net_piple_bind(server_id, client_id, piple_id, callback);
	}
	return 0;
}