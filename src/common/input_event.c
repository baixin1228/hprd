#include <malloc.h>
#include <string.h>
#include <netinet/in.h>

#include "util.h"
#include "common.h"
#include "net_help.h"
#include "protocol.h"
#include "tcp_client.h"
#include "input_event.h"

struct data_pkt *event_pkt = NULL;

int send_event(int fd, uint32_t cmd, char *buf, size_t len)
{
	if(!event_pkt)
	{
		event_pkt = calloc(1, sizeof(struct data_pkt) +
			sizeof(struct input_event));
	}

	event_pkt->channel = cmd;
	memcpy(event_pkt->data, buf, len);
	event_pkt->data_len = htonl(len);

	if(tcp_send_pkt(fd, (char *)event_pkt, sizeof(struct data_pkt) +
		len) == -1)
	{
		return -1;
	}

	return 0;
}

// int release()
// {
// 	if(event_pkt)
// 	{
// 		free(event_pkt);
// 		event_pkt = NULL;
// 	}
// }