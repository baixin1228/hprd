#include <malloc.h>
#include <string.h>
#include <netinet/in.h>

#include "util.h"
#include "common.h"
#include "net_help.h"
#include "protocol.h"
#include "tcp_client.h"
#include "protocol.h"

int server_send_event(struct ep_event *ev, uint32_t cmd, char *buf, size_t len)
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

	if(server_send_data_safe(ev, (char *)event_pkt, sizeof(struct data_pkt) +
		len) == -1)
	{
		return -1;
	}

	return 0;
}

void bradcast_video(char *buf, size_t len)
{
	static struct data_pkt *video_pkt = NULL;

	if(!video_pkt)
	{
		video_pkt = calloc(1, 10 * 1024 * 1024);
	}
	
	video_pkt->channel = VIDEO_CHANNEL;
	video_pkt->data_len = htonl(len);
	memcpy(video_pkt->data, buf, len);
	server_bradcast_data((char *)video_pkt, len + sizeof(*video_pkt));
}