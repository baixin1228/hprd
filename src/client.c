#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "util.h"
#include "codec.h"
#include "decodec.h"
#include "tcp_client.h"
#include "display_dev.h"
#include "buffer_pool.h"

struct decodec_object *dec_obj = NULL;

void on_package(char *buf, size_t len)
{
	if(dec_obj)
	{
		decodec_put_pkt(dec_obj, buf, len);
		log_info("buf:%dkb", len / 1024);
	}
}

void display_on_event(struct display_object *obj)
{
	int ret;
	int buf_id;

	buf_id = decodec_get_fb(dec_obj);
	if(buf_id == -1)
	{
		return;
	}

	ret = display_put_fb(obj, buf_id);
	if(ret != 0)
	{
		log_error("display_put_fb fail.");
		exit(-1);
	}
	buf_id = display_get_fb(obj);
	if(buf_id == -1)
	{
		log_error("display_get_fb fail.");
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
struct display_object *out_obj = NULL;
void *client_thread(void *opaque)
{
	int ret;
	int buf_id;
	uint32_t frame_rate = 61;
	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	dec_obj = decodec_init(&client_pool);
	out_obj = display_dev_init(&client_pool);

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
		ret = display_map_fb(out_obj, buf_id);
		if(ret != 0)
		{
			log_error("display_map_fb fail.");
			exit(-1);
		}
	}

	while(decodec_get_info(dec_obj, fb_info) != 0)
	{
		log_info("client waiting dec info.");
		sleep(1);
	}

	display_regist_event_callback(out_obj, display_on_event);
	g_hash_table_insert(fb_info, "frame_rate", &frame_rate);
	display_set_info(out_obj, fb_info);
	
	g_hash_table_destroy(fb_info);

	display_main_loop(out_obj);
	return NULL;
}

int main()
{
	pthread_t p1;
	pthread_t p2;
	pthread_create(&p1, NULL, client_thread, NULL);
	pthread_create(&p2, NULL, tcp_client_thread, NULL);
	pthread_join(p1, NULL);
	pthread_join(p2, NULL);
}