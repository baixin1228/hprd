#include <stdlib.h>

#include "util.h"
#include "codec.h"
#include "encodec.h"
#include "input_dev.h"
#include "output_dev.h"
#include "buffer_pool.h"

struct output_object *out_obj = NULL;
struct encodec_object *enc_obj = NULL;
struct input_object *in_obj = NULL;

void on_event(struct output_object *obj)
{
	int ret;
	int buf_id;

	buf_id = input_get_fb(in_obj);
	if(buf_id == -1)
	{
		log_error("input_get_fb fail.");
		exit(-1);
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

void on_package(char *buf, size_t len)
{
	log_info("buf:%d", len);
}

int main()
{
	int ret;
	int buf_id;
	uint32_t stream_ftm = STREAM_H264;
	uint32_t bit_rate = 1 * 1024 * 1024;
	GHashTable *fb_info;

	fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	out_obj = output_dev_init();
	in_obj = input_dev_init();
	enc_obj = encodec_init();
	output_regist_event_callback(out_obj, on_event);
	encodec_regist_event_callback(enc_obj, on_package);

	input_get_info(in_obj, fb_info);
	output_set_info(out_obj, fb_info);
	
	g_hash_table_insert(fb_info, "stream_fmt", &stream_ftm);
	g_hash_table_insert(fb_info, "bit_rate", &bit_rate);
	
	encodec_set_info(enc_obj, fb_info);

	g_hash_table_destroy(fb_info);

	for (int i = 0; i < 5; ++i)
	{
		buf_id = alloc_buffer();
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
		ret = output_map_fb(out_obj, buf_id);
		if(ret != 0)
		{
			log_error("output_map_fb fail.");
			exit(-1);
		}
	}
	output_main_loop(out_obj);
	return 0;
}