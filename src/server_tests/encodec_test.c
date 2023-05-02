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
#include "net/tcp_server.h"
#include "buffer_pool.h"

struct mem_pool server_pool = {0};

struct encodec_object *enc_obj = NULL;
struct capture_object *cap_obj = NULL;
struct input_object *in_obj = NULL;

void capture_on_frame(struct capture_object *obj)
{
	int ret;
	int buf_id;

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

FILE *fp = NULL;
uint32_t frames = 0;
void on_enc_package(char *buf, size_t len)
{
	if(fp)
	{
		log_info("write frame");
		fwrite(buf, len, 1, fp);
		frames++;
		if(frames > 200)
			exit(0);
	}
}

int server_start()
{
	int ret;
	int buf_id;
	uint32_t frame_rate = 60;
	uint32_t stream_ftm = STREAM_H264;
	uint32_t bit_rate = 10 * 1024 * 1024;

	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	cap_obj = capture_dev_init(&server_pool, NULL);
	enc_obj = encodec_init(&server_pool, "openh264_encodec");
	in_obj = input_init(NULL);

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
	if(capture_set_info(cap_obj, fb_info) == -1)
	{
		log_error("capture_set_info fail.");
		exit(-1);
	}
	capture_regist_event_callback(cap_obj, capture_on_frame);
	capture_get_info(cap_obj, fb_info);

	encodec_regist_event_callback(enc_obj, on_enc_package);
	g_hash_table_insert(fb_info, "stream_fmt", &stream_ftm);
	g_hash_table_insert(fb_info, "bit_rate", &bit_rate);
	if(encodec_set_info(enc_obj, fb_info) == -1)
	{
		log_error("encodec_set_info fail.");
		exit(-1);
	}

	g_hash_table_destroy(fb_info);

	capture_main_loop(cap_obj);
	return 0;
}

int main()
{
	debug_info_regist();
	log_info("start encodec test.");
	fp = fopen("out.h264", "wb");
	server_start();
}