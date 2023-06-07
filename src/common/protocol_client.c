#include <malloc.h>
#include <string.h>
#include <netinet/in.h>

#include "util.h"
#include "common.h"
#include "protocol.h"
#include "net/net_client.h"


int send_input_event(char *buf, size_t len)
{
	static struct data_pkt *event_pkt = NULL;
	if(!event_pkt)
	{
		event_pkt = calloc(1, sizeof(struct data_pkt) +
			sizeof(struct input_event));
	}

	event_pkt->channel = INPUT_CHANNEL;
	memcpy(event_pkt->data, buf, len);
	event_pkt->data_len = htonl(len);

	if(client_send_pkt((char *)event_pkt, sizeof(struct data_pkt) +
		len) == -1)
	{
		return -1;
	}

	return 0;
}

int send_request_event(uint32_t cmd, uint32_t value, uint32_t id)
{
	struct request_event *req_event;
	static struct data_pkt *event_pkt = NULL;
	int pkt_len = sizeof(*event_pkt) + sizeof(*req_event);

	if(!event_pkt)
	{
		event_pkt = calloc(1, pkt_len);
	}
	req_event = (struct request_event *)event_pkt->data;
	req_event->cmd = cmd;
	req_event->id = htonl(id);
	req_event->value = htonl(value);

	event_pkt->channel = REQUEST_CHANNEL;
	event_pkt->data_len = htonl(sizeof(*req_event));

	if(client_send_pkt((char *)event_pkt, pkt_len) == -1)
	{
		return -1;
	}

	return 0;
}

int send_clip_event(char *type, char *data, uint16_t len)
{
	int send_len = len;
	struct clip_event *clip_event = NULL;
	static struct data_pkt *event_pkt = NULL;

	if(len > 10200)
		return -1;

	if(!event_pkt)
	{
		event_pkt = calloc(1, 10240);
	}

	clip_event = (struct clip_event *)event_pkt->data;
	send_len += sizeof(*clip_event);
	clip_event->data_len = htons(len + strlen(type) + 1);
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Warray-bounds"
	strcpy(clip_event->clip_data, type);
	#pragma GCC diagnostic pop
	send_len += strlen(type) + 1;
	memcpy(&clip_event->clip_data[strlen(type) + 1], data, len);

	event_pkt->channel = CLIP_CHANNEL;
	event_pkt->data_len = htonl(send_len);
	
	send_len += sizeof(*event_pkt);

	if(client_send_pkt((char *)event_pkt, send_len) == -1)
	{
		return -1;
	}

	return 0;
}