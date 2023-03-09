#include <malloc.h>
#include <string.h>

#include "util.h"
#include "tcp_client.h"
#include "protocol.h"
#include "input_event.h"

int hsend_event(struct input_event *event)
{
	struct data_pkt *pkt = calloc(1, sizeof(struct data_pkt) +
		sizeof(struct input_event));

	pkt->cmd = INPUT_EVENT;
	memcpy(pkt->data, event, sizeof(struct input_event));
	pkt->data_len = sizeof(struct input_event);

	if(client_send_pkt((char *)pkt, sizeof(struct data_pkt) +
		sizeof(struct input_event)) == -1)
	{
		log_error("client_send_pkt fail.");
		exit(-1);
	}
	free(pkt);
	return 0;
}