#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
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
pthread_mutex_t net_cb_lock = PTHREAD_MUTEX_INITIALIZER;

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
	if(in_obj)
		input_push_key(in_obj, event);
}

#define MIN_BIT_RATE (1 * 1024 * 1024)
uint32_t frame_rate = 58;
uint32_t stream_ftm = STREAM_H264;
uint32_t bit_rate = MIN_BIT_RATE;
void on_setting(struct ep_event *ev, struct setting_event *event)
{
	struct setting_event ret_event;

	ret_event.set_id = event->set_id;
	switch(event->cmd)
	{
		case SET_BIT_RATE:
		{
			bit_rate = ntohl(event->value);
			if(bit_rate < MIN_BIT_RATE)
				bit_rate = MIN_BIT_RATE;
			capture_quit(cap_obj);
			ret_event.cmd = RET_SUCCESS;
			ret_event.value = event->value;
			log_info("change bit rate.");
			break;
		}
		case SET_FRAME_RATE:
		{
			frame_rate = ntohl(event->value);
			capture_quit(cap_obj);
			ret_event.cmd = RET_SUCCESS;
			ret_event.value = event->value;
			log_info("change frame rate.");
			break;
		}
		case GET_BIT_RATE:
		{
			ret_event.cmd = RET_SUCCESS;
			ret_event.value = htonl(bit_rate);
			log_info("get bit rate.");
			break;
		}
		case GET_FRAME_RATE:
		{
			ret_event.cmd = RET_SUCCESS;
			ret_event.value = htonl(frame_rate);
			log_info("get frame rate.");
			break;
		}
		default:
		{
			log_warning("setting: unknow cmd.");
			break;
		}
	}

	if(ret_event.cmd != RET_SUCCESS)
		ret_event.cmd = RET_FAIL;

	if(server_send_event(ev, SETTING_CHANNEL, (char *)&ret_event, sizeof(ret_event))
			== -1) {
		log_error("send_event fail.");
	}
}

void on_server_pkt(struct ep_event *ev, char *buf, size_t len) {
	struct data_pkt *pkt = (struct data_pkt *)buf;
	pthread_mutex_lock(&net_cb_lock);
	switch(pkt->channel) {
		case INPUT_CHANNEL: {
			on_key((struct input_event *)pkt->data);
			break;
		}
		case SETTING_CHANNEL: {
			on_setting(ev, (struct setting_event *)pkt->data);
			break;
		}
		default: {
			break;
		}
	}
	pthread_mutex_unlock(&net_cb_lock);
}

void on_enc_package(char *buf, size_t len)
{
	bradcast_video(buf, len);
}

int server_start(char *capture, char *encodec)
{
	int ret;
	int buf_id;
	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	if(encodec == NULL)
		encodec = "ffmpeg_encodec";

	cap_obj = capture_dev_init(&server_pool);
	enc_obj = encodec_init(&server_pool, encodec);
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
			log_error("encodec_map_fb fail.");
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

	pthread_mutex_lock(&net_cb_lock);
	for (int i = 0; i < 5; ++i)
	{
		buf_id = get_buffer(&server_pool);
		if(buf_id == -1)
		{
			log_error("alloc_buffer fail.");
			exit(-1);
		}
		ret = capture_unmap_fb(cap_obj, buf_id);
		if(ret != 0)
		{
			log_error("capture_unmap_fb fail.");
			exit(-1);
		}
		ret = encodec_unmap_fb(enc_obj, buf_id);
		if(ret != 0)
		{
			log_error("encodec_unmap_fb fail.");
			exit(-1);
		}
		release_buffer(&server_pool, buf_id);
	}

	input_release(in_obj);
	encodec_release(enc_obj);
	capture_release(cap_obj);
	in_obj = NULL;
	enc_obj = NULL;
	cap_obj = NULL;
	pthread_mutex_unlock(&net_cb_lock);
	return 0;
}

struct option long_options[] =
{
	{"help",  	no_argument,       NULL, 'h'},
	{"capture", required_argument, NULL, 'c'},
	{"encodec", required_argument, NULL, 'e'},
	{NULL,		0,                 NULL,  0}
};

void print_help()
{
	printf("help\n");
}

int main(int argc, char* argv[])
{
    int ret = -1;
    char *capture = NULL;
    char *encodec = NULL;
    int option_index = 0;

	debug_info_regist();

    while ((ret = getopt_long(argc, argv, "h", long_options, &option_index)) != -1)
    {
    	switch(ret)
    	{
    		case 0:
    		case 'h':
    		default:
    		{
    			print_help();
    			exit(0);
    			break;
    		}
    		case 1:
    		case 'c':
    		{
    			capture = optarg;
    			break;
    		}
    		case 2:
    		case 'e':
    		{
    			encodec = optarg;
    			break;
    		}
    	}
    }

	tcp_server_init("0.0.0.0", 9999);
	while(1)
	{
		server_start(capture, encodec);
		log_info("restart server.");
	}
}