#include <malloc.h>
#include <string.h>
#include <netinet/in.h>

#include "util.h"
#include "common.h"
#include "net_help.h"
#include "protocol.h"
#include "tcp_client.h"
#include "protocol.h"


int send_event(int fd, uint32_t cmd, char *buf, size_t len)
{
	static struct data_pkt *event_pkt = NULL;
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