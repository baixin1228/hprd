#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "util.h"
#include "codec.h"
#include "encodec.h"
#include "capture_dev.h"
#include "tcp_server.h"
#include "buffer_pool.h"

struct mem_pool server_pool = {0};

struct encodec_object *enc_obj = NULL;
struct input_object *in_obj = NULL;

void input_on_event(struct input_object *obj)
{
	int ret;
	int buf_id;

	buf_id = input_get_fb(in_obj);
	if(buf_id == -1)
	{
		log_error("input_get_fb fail.");
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

	ret = input_put_fb(in_obj, buf_id);
	if(ret != 0)
	{
		log_error("input_put_fb fail.");
		exit(-1);
	}
}

extern char video_pkt[1024 * 1024 * 10];
extern uint32_t video_pkt_len;
extern pthread_spinlock_t video_pkt_lock;

void on_package(char *buf, size_t len)
{
	bradcast_data2(buf, len);
	// pthread_spin_lock(&video_pkt_lock);
	// memcpy(video_pkt, buf, len);
	// video_pkt_len = len;
	// log_info("buf:%dkb", len / 1024);
	// pthread_spin_unlock(&video_pkt_lock);
}

void *server_thread(void *opaque)
{
	int ret;
	int buf_id;
	uint32_t frame_rate = 60;
	uint32_t stream_ftm = STREAM_H264;
	uint32_t bit_rate = 100 * 1024 * 1024;

	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	in_obj = capture_dev_init(&server_pool);
	enc_obj = encodec_init(&server_pool);

	for (int i = 0; i < 5; ++i)
	{
		buf_id = alloc_buffer(&server_pool);
		if(buf_id == -1)
		{
			log_error("alloc_buffer fail.");
			exit(-1);
		}
		ret = input_map_fb(in_obj, buf_id);
		if(ret != 0)
		{
			log_error("input_map_fb fail.");
			exit(-1);
		}
		ret = encodec_map_fb(enc_obj, buf_id);
		if(ret != 0)
		{
			log_error("output_map_fb fail.");
			exit(-1);
		}
	}

	g_hash_table_insert(fb_info, "frame_rate", &frame_rate);
	input_set_info(in_obj, fb_info);
	input_regist_event_callback(in_obj, input_on_event);
	input_get_info(in_obj, fb_info);

	encodec_regist_event_callback(enc_obj, on_package);
	g_hash_table_insert(fb_info, "stream_fmt", &stream_ftm);
	g_hash_table_insert(fb_info, "bit_rate", &bit_rate);
	encodec_set_info(enc_obj, fb_info);

	g_hash_table_destroy(fb_info);

	input_main_loop(in_obj);
	return NULL;
}

int main()
{
	pthread_t p1;
	pthread_t p2;

	pthread_spin_init(&video_pkt_lock, 0);

	pthread_create(&p1, NULL, server_thread, NULL);
	pthread_create(&p2, NULL, tcp_server_thread, NULL);
	pthread_join(p1, NULL);
	pthread_join(p2, NULL);
}