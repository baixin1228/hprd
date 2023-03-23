#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>

#include "util.h"
#include "codec.h"
#include "encodec.h"
#include "protocol.h"
#include "capture_dev.h"
#include "input_dev.h"
#include "tcp_server.h"
#include "buffer_pool.h"

struct mem_pool server_pool = {0};

struct encodec_object *enc_obj = NULL;
struct capture_object *cap_obj = NULL;
struct input_object *in_obj = NULL;

void capture_on_frame(struct capture_object *obj)
{
	int ret;
	int buf_id;

	if(get_client_count() == 0)
		return;

	buf_id = capture_get_fb(cap_obj);
	if(buf_id == -1)
	{
		log_error("capture_get_fb fail.");
		exit(-1);
	}

	ret = encodec_put_fb(enc_obj, buf_id);
	if(ret != 0)
	{
		log_error("encodec_put_fb fail.");
		exit(-1);
	}
	buf_id = encodec_get_fb(enc_obj);
	if(buf_id == -1)
	{
		log_error("encodec_get_fb fail.");
		exit(-1);
	}

	ret = capture_put_fb(cap_obj, buf_id);
	if(ret != 0)
	{
		log_error("capture_put_fb fail.");
		exit(-1);
	}
}

void on_key(struct input_event *event)
{
	input_push_key(in_obj, event);
}

void on_setting(struct setting_event *event)
{
	switch(event->cmd)
	{
		case TARGET_BIT_RATE:
		{
			break;
		}
		case TARGET_FRAME_RATE:
		{
			capture_change_frame_rate(cap_obj, ntohl(event->value));
			log_info("change frame rate.");
			break;
		}
	}
}

void on_server_pkt(char *buf, size_t len) {
	struct data_pkt *pkt = (struct data_pkt *)buf;
	switch(pkt->cmd) {
		case INPUT_EVENT: {
			on_key((struct input_event *)pkt->data);
			break;
		}
		case SETTING_EVENT: {
			on_setting((struct setting_event *)pkt->data);
			break;
		}
	}
}

int epfd = -1;
void on_enc_package(char *buf, size_t len)
{
	if(epfd != -1)
		server_bradcast_data_safe(epfd, buf, len);
}

void *server_thread(void *opaque)
{
	int ret;
	int buf_id;
	uint32_t frame_rate = 60;
	uint32_t stream_ftm = STREAM_H264;
	uint32_t bit_rate = 10 * 1024 * 1024;

	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	cap_obj = capture_dev_init(&server_pool);
	enc_obj = encodec_init(&server_pool);
	in_obj = input_init();

	for (int i = 0; i < 5; ++i)
	{
		buf_id = alloc_buffer(&server_pool);
		if(buf_id == -1)
		{
			log_error("alloc_buffer fail.");
			exit(-1);
		}
		ret = capture_map_fb(cap_obj, buf_id);
		if(ret != 0)
		{
			log_error("capture_map_fb fail.");
			exit(-1);
		}
		ret = encodec_map_fb(enc_obj, buf_id);
		if(ret != 0)
		{
			log_error("display_map_fb fail.");
			exit(-1);
		}
	}

	g_hash_table_insert(fb_info, "frame_rate", &frame_rate);
	capture_set_info(cap_obj, fb_info);
	capture_regist_event_callback(cap_obj, capture_on_frame);
	capture_get_info(cap_obj, fb_info);

	encodec_regist_event_callback(enc_obj, on_enc_package);
	g_hash_table_insert(fb_info, "stream_fmt", &stream_ftm);
	g_hash_table_insert(fb_info, "bit_rate", &bit_rate);
	encodec_set_info(enc_obj, fb_info);

	g_hash_table_destroy(fb_info);

	capture_main_loop(cap_obj);
	return NULL;
}

int main()
{
	pthread_t p1;

	pthread_create(&p1, NULL, server_thread, NULL);
	epfd = tcp_server_init("0.0.0.0", 9999);
	pthread_join(p1, NULL);
}