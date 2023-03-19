#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "util.h"
#include "codec.h"
#include "decodec.h"
#include "tcp_client.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "protocol.h"
#include "net_help.h"

int fd = -1;
struct mem_pool client_pool = {0};
struct display_object *dsp_obj = NULL;
struct decodec_object *dec_obj = NULL;

static void _on_event(struct display_object *obj, struct input_event *event)
{
	struct data_pkt *pkt = calloc(1, sizeof(struct data_pkt) +
		sizeof(struct input_event));

	pkt->cmd = INPUT_EVENT;
	memcpy(pkt->data, event, sizeof(struct input_event));
	pkt->data_len = htonl(sizeof(struct input_event));

	if(client_send_pkt(fd, (char *)pkt, sizeof(struct data_pkt) +
		sizeof(struct input_event)) == -1)
	{
		log_error("client_send_pkt fail.");
		exit(-1);
	}
	free(pkt);
}

static void _on_package(char *buf, size_t len)
{
	if(dec_obj)
	{
		decodec_put_pkt(dec_obj, buf, len);
	}
}

static void _on_client_pkt(char *buf, size_t len)
{
	struct data_pkt *pkt = (struct data_pkt *)buf;
	pkt->data_len = ntohl(pkt->data_len);

	switch(pkt->cmd)
	{
		case VIDEO_DATA:
		{
			_on_package(pkt->data, pkt->data_len);
			break;
		}
	}
}

#define BUFLEN 1024 * 1024 * 10
static char _recv_buf[BUFLEN];
int py_on_frame()
{
	int ret;
	int buf_id;

	if(tcp_recv_pkt(fd, _recv_buf, _on_client_pkt) == -1)
		return 0;

	buf_id = decodec_get_fb(dec_obj);
	if(buf_id == -1)
	{
		return 0;
	}

	ret = display_put_fb(dsp_obj, buf_id);
	if(ret != 0)
	{
		log_error("display_put_fb fail.");
		return -1;
	}
	buf_id = display_get_fb(dsp_obj);
	if(buf_id == -1)
	{
		log_error("display_get_fb fail.");
		return -1;
	}

	ret = decodec_put_fb(dec_obj, buf_id);
	if(ret != 0)
	{
		log_error("decodec_put_fb fail.");
		return -1;
	}

	return 0;
}

int py_client_init_config(uint64_t winid)
{
	int ret = -1;
	uint32_t frame_rate = 61;
	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	if(tcp_recv_pkt(fd, _recv_buf, _on_client_pkt) == -1)
		return -1;

	if(decodec_get_info(dec_obj, fb_info) == -1)
	{
		log_info("client waiting dec info.");
		goto END;
	}

	display_regist_event_callback(dsp_obj, _on_event);
	g_hash_table_insert(fb_info, "frame_rate", &frame_rate);
	g_hash_table_insert(fb_info, "window", &winid);
	display_set_info(dsp_obj, fb_info);
	ret = 0;
END:
	g_hash_table_destroy(fb_info);
	return ret;
}

int py_client_connect(char *ip, uint16_t port)
{
	int ret;
	int buf_id;

	debug_info_regist();
	
	fd = client_connect(ip, port);

	if (fd < 0) {
		return -1;
	}

	dec_obj = decodec_init(&client_pool);
	dsp_obj = display_dev_init(&client_pool, "x11_renderer");

	for (int i = 0; i < 5; ++i)
	{
		buf_id = alloc_buffer(&client_pool);
		if(buf_id == -1)
		{
			log_error("alloc_buffer fail.");
			return -1;
		}
		ret = decodec_map_fb(dec_obj, buf_id);
		if(ret != 0)
		{
			log_error("decodec_map_fb fail.");
			return -1;
		}
		ret = display_map_fb(dsp_obj, buf_id);
		if(ret != 0)
		{
			log_error("display_map_fb fail.");
			return -1;
		}
	}

	return 0;
}

int py_client_resize(uint32_t width, uint32_t height)
{
	if(!dsp_obj)
		return -1;

	return display_resize(dsp_obj, width, height);
}

int py_client_close()
{
	if(fd != -1)
	{
		close(fd);
		fd = -1;
	}

	return 0;
}

int py_mouse_move(int x, int y)
{
	struct input_event event;

	event.type = MOUSE_MOVE;
	event.x = x;
	event.y = y;

	if(send_event(fd, INPUT_EVENT, (char *)&event, sizeof(struct input_event))
		== -1)
	{
		log_error("send_event fail.");
		return -1;
	}

	return 0;
}

int py_mouse_click(int x, int y, int key, int down_or_up)
{
	struct input_event event;

	if(down_or_up)
	{
		event.type = MOUSE_DOWN;
	}else{
		event.type = MOUSE_UP;
	}
	
	event.key_code = key;
	event.x = x;
	event.y = y;

	if(send_event(fd, INPUT_EVENT, (char *)&event, sizeof(struct input_event))
		== -1)
	{
		log_error("send_event fail.");
		return -1;
	}

	return 0;
}