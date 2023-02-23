#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "util.h"
#include "codec.h"
#include "encodec.h"
#include "decodec.h"
#include "input_dev.h"
#include "output_dev.h"
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

struct decodec_object *dec_obj = NULL;

void on_package(char *buf, size_t len)
{
	if(dec_obj)
	{
		decodec_put_pkt(dec_obj, buf, len);
		// log_info("buf:%dkb", len / 1024);
	}
}

void *server_thread(void *opaque)
{
	int ret;
	int buf_id;
	uint32_t frame_rate = 60;
	uint32_t stream_ftm = STREAM_H264;
	uint32_t bit_rate = 100 * 1024 * 1024;

	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	in_obj = input_dev_init(&server_pool);
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

void output_on_event(struct output_object *obj)
{
	int ret;
	int buf_id;

	buf_id = decodec_get_fb(dec_obj);
	if(buf_id == -1)
	{
		return;
	}

	ret = output_put_fb(obj, buf_id);
	if(ret != 0)
	{
		log_error("output_put_fb fail.");
		exit(-1);
	}
	buf_id = output_get_fb(obj);
	if(buf_id == -1)
	{
		log_error("output_get_fb fail.");
		exit(-1);
	}

	ret = decodec_put_fb(dec_obj, buf_id);
	if(ret != 0)
	{
		log_error("decodec_put_fb fail.");
		exit(-1);
	}
}

struct mem_pool client_pool = {0};
struct output_object *out_obj = NULL;
void *client_thread(void *opaque)
{
	int ret;
	int buf_id;
	uint32_t frame_rate = 61;
	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	dec_obj = decodec_init(&client_pool);
	out_obj = output_dev_init(&client_pool);

	for (int i = 0; i < 5; ++i)
	{
		buf_id = alloc_buffer(&client_pool);
		if(buf_id == -1)
		{
			log_error("alloc_buffer fail.");
			exit(-1);
		}
		ret = decodec_map_fb(dec_obj, buf_id);
		if(ret != 0)
		{
			log_error("decodec_map_fb fail.");
			exit(-1);
		}
		ret = output_map_fb(out_obj, buf_id);
		if(ret != 0)
		{
			log_error("output_map_fb fail.");
			exit(-1);
		}
	}

	while(decodec_get_info(dec_obj, fb_info) != 0)
	{
		log_info("client waiting dec info.");
		sleep(1);
	}

	output_regist_event_callback(out_obj, output_on_event);
	g_hash_table_insert(fb_info, "frame_rate", &frame_rate);
	output_set_info(out_obj, fb_info);
	
	g_hash_table_destroy(fb_info);

	output_main_loop(out_obj);
	return NULL;
}

int main()
{
	pthread_t p1, p2;
	pthread_create(&p1, NULL, server_thread, NULL);
	pthread_create(&p2, NULL, client_thread, NULL);
	pthread_join(p1, NULL);
	pthread_join(p2, NULL);
}