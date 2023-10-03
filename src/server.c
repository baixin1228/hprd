#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "util.h"
#include "codec.h"
#include "encodec.h"
#include "protocol.h"
#include "input_dev.h"
#include "capture_dev.h"
#include "buffer_pool.h"
#include "frame_buffer.h"
#include "net/net_server.h"

struct mem_pool server_pool = {0};

struct encodec_object *enc_obj = NULL;
struct capture_object *cap_obj = NULL;
struct input_object *in_obj = NULL;
pthread_mutex_t net_cb_lock = PTHREAD_MUTEX_INITIALIZER;

struct timeval time1, time2;
static void capture_on_frame(struct capture_object *obj)
{
	int ret;
	int buf_id;
	uint64_t capture_time;
	uint64_t enc_time;

	if(get_client_count() == 0)
	{
		sleep(1);
		return;
	}

	gettimeofday(&time1, NULL);
	buf_id = capture_get_fb(cap_obj);
	if(buf_id == -1)
	{
		log_error("capture_get_fb fail.");
		exit(-1);
	}
	gettimeofday(&time2, NULL);
	capture_time = time2.tv_usec + time2.tv_sec * 1000000 - time1.tv_usec - time1.tv_sec * 1000000;

	ret = encodec_put_fb(enc_obj, buf_id);
	if(ret != 0)
	{
		log_error("encodec_put_fb fail.");
		exit(-1);
	}
	gettimeofday(&time1, NULL);
	enc_time = time1.tv_usec + time1.tv_sec * 1000000 - time2.tv_usec - time2.tv_sec * 1000000;

	log_info("capture_time:%dms enc_time:%dms", capture_time / 1000, enc_time / 1000);
}

static void on_key(struct input_event *event)
{
	if(in_obj)
		input_push_key(in_obj, event);
}

static void on_clip(struct clip_event *event)
{
	if(in_obj)
		input_push_clip(in_obj, event);
}

#define MIN_BIT_RATE (1 * 1024 * 1024)
static uint32_t frame_rate = 58;
float frame_scale = 1.0f;
static uint32_t stream_ftm = STREAM_H264;
static uint32_t bit_rate = MIN_BIT_RATE;
static void on_request(struct server_client *client, struct request_event *event)
{
	struct response_event ret_event;
	uint32_t req_value = ntohl(event->value);

	ret_event.id = event->id;
	switch(event->cmd)
	{
		case SET_BIT_RATE:
		{
			bit_rate = req_value;
			if(bit_rate < MIN_BIT_RATE)
				bit_rate = MIN_BIT_RATE;
			capture_quit(cap_obj);
			ret_event.ret = RET_SUCCESS;
			ret_event.value = event->value;
			log_info("change bit rate.");
			break;
		}
		case SET_FRAME_RATE:
		{
			frame_rate = req_value;
			capture_quit(cap_obj);
			ret_event.ret = RET_SUCCESS;
			ret_event.value = event->value;
			log_info("change frame rate.");
			break;
		}
		case SET_SHARE_CLIPBOARD:
		{
			ret_event.ret = RET_SUCCESS;
			ret_event.value = event->value;
			log_info("set share clipboard:%s.", req_value == 1 ? "true" : "false");
			break;
		}
		case GET_BIT_RATE:
		{
			ret_event.ret = RET_SUCCESS;
			ret_event.value = htonl(bit_rate);
			log_info("get bit rate.");
			break;
		}
		case GET_FRAME_RATE:
		{
			ret_event.ret = RET_SUCCESS;
			ret_event.value = htonl(frame_rate);
			log_info("get frame rate.");
			break;
		}
		case GET_FPS:
		{
			ret_event.ret = RET_SUCCESS;
			ret_event.value = htonl((uint32_t)capture_get_fps(cap_obj));
			break;
		}
		case GET_CLIENT_ID:
		{
			if(server_client_getid(client) != 0)
			{
				ret_event.ret = RET_SUCCESS;
				ret_event.value = htonl(server_client_getid(client));
			}
			log_info("get frame rate.");
			break;
		}
		case PING:
		{
			ret_event.ret = RET_SUCCESS;
			ret_event.value = event->value;
			break;
		}
		default:
		{
			log_warning("setting: unknow cmd.");
			break;
		}
	}

	if(ret_event.ret != RET_SUCCESS)
		ret_event.ret = RET_FAIL;

	if(server_send_event(client, RESPONSE_CHANNEL, (char *)&ret_event, sizeof(ret_event)) == -1)
	{
		log_error("send_event fail.");
	}
}

static void on_server_pkt(struct server_client *client, char *buf, size_t len)
{
	struct data_pkt *pkt = (struct data_pkt *)buf;
	pthread_mutex_lock(&net_cb_lock);
	switch(pkt->channel)
	{
		case INPUT_CHANNEL:
		{
			on_key((struct input_event *)pkt->data);
			break;
		}
		case CLIP_CHANNEL:
		{
			on_clip((struct clip_event *)pkt->data);
			break;
		}
		case REQUEST_CHANNEL:
		{
			on_request(client, (struct request_event *)pkt->data);
			break;
		}
		default:
		{
			break;
		}
	}
	pthread_mutex_unlock(&net_cb_lock);
}

void on_client_connect(struct server_client *client)
{
	if(enc_obj)
	{
		encodec_force_i(enc_obj);
	}
}

pthread_t bradcast_thread;
static char *enc_data;
static size_t enc_data_len;
static void* bradcast_video_thread(void * data)
{
	while(true)
	{
		if(enc_data_len != 0)
		{
			bradcast_video(enc_data, enc_data_len);
			enc_data_len = 0;
		} else {
			usleep(1000);
		}
	}

	return NULL;
}

void on_enc_package(char *buf, size_t len)
{
	while(enc_data_len != 0)
		usleep(1000);

	memcpy(enc_data, buf, len);
	enc_data_len = len;
}

int server_start(char *capture, char *encodec)
{
	int ret;
	int buf_id;
	bool share_mem;
	uint32_t screen_width, screen_height, screen_format;
	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	if(encodec == NULL)
		encodec = "ffmpeg_encodec";

	cap_obj = capture_dev_init(&server_pool, capture);
	enc_obj = encodec_init(&server_pool, encodec);
	in_obj = input_init(NULL);

	share_mem = cap_obj->ops->priority & SHARE_MEMORY && enc_obj->ops->priority & SHARE_MEMORY;
	
	log_info("share memory:%d", share_mem);

	ret = capture_get_info(cap_obj, fb_info);
	if(ret != 0)
	{
		log_error("capture_get_info fail.");
		return -1;
	}
	screen_width = *(uint32_t *)g_hash_table_lookup(fb_info, "width");
	screen_height = *(uint32_t *)g_hash_table_lookup(fb_info, "height");
	screen_format = *(uint32_t *)g_hash_table_lookup(fb_info, "format");
	log_info("screen info, width:%d height:%d format:%s", screen_width, screen_height, format_to_name(screen_format));

	for (int i = 0; i < 5; ++i)
	{
		// buf_id = alloc_buffer2(&server_pool, screen_width, screen_height, screen_format);
		buf_id = alloc_buffer(&server_pool);
		if(buf_id == -1)
		{
			log_error("alloc_buffer fail.");
			return -1;
		}
		if (share_mem)
		{
			ret = capture_map_fb(cap_obj, buf_id);
			if(ret != 0)
			{
				log_error("capture_map_fb fail.");
				return -1;
			}
			ret = encodec_map_fb(enc_obj, buf_id);
			if(ret != 0)
			{
				log_error("encodec_map_fb fail.");
				return -1;
			}
		}
	}


	g_hash_table_insert(fb_info, "frame_rate", &frame_rate);
	if(capture_set_info(cap_obj, fb_info) == -1)
	{
		log_error("capture_set_info fail.");
		goto out1;
	}
	capture_regist_event_callback(cap_obj, capture_on_frame);
	capture_get_info(cap_obj, fb_info);

	encodec_regist_event_callback(enc_obj, on_enc_package);
	g_hash_table_insert(fb_info, "stream_fmt", &stream_ftm);
	g_hash_table_insert(fb_info, "bit_rate", &bit_rate);
	g_hash_table_insert(fb_info, "frame_scale", &frame_scale);
	if(encodec_set_info(enc_obj, fb_info) == -1)
	{
		log_error("encodec_set_info fail.");
		goto out1;
	}

	g_hash_table_destroy(fb_info);

	capture_main_loop(cap_obj);

	pthread_mutex_lock(&net_cb_lock);

out1:
	for (int i = 0; i < 5; ++i)
	{
		buf_id = get_fb_block(&server_pool, 100);
		if(buf_id == -1)
		{
			log_error("%s get_fb time out", __func__);
			exit(-1);
		}

		if (share_mem)
		{
			ret = capture_unmap_fb(cap_obj, buf_id);
			if(ret != 0)
			{
				log_error("capture_unmap_fb fail. id:%d", buf_id);
				return -1;
			}
			ret = encodec_unmap_fb(enc_obj, buf_id);
			if(ret != 0)
			{
				log_error("encodec_unmap_fb fail.");
				return -1;
			}
		}

		if(release_buffer(&server_pool, buf_id) == -1)
		{
			log_error("%s release buffer fail.", __func__);
			exit(-1);
		}
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
	{"help",  		no_argument,        NULL, 'h'},
	{"capture", 	required_argument,  NULL, 'c'},
	{"encodec", 	required_argument,  NULL, 'e'},
	{"kcp-disable", no_argument,        NULL, 'k'},
	{"scale", 		required_argument,  NULL, 's'},
	{NULL,		0,  	                NULL,  0}
};

void print_help()
{
	printf("help");
}

int main(int argc, char *argv[])
{
	int ret = -1;
	char *capture = NULL;
	char *encodec = NULL;
	int option_index = 0;
	bool enable_kcp = true;

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
			case 3:
			case 'k':
			{
				enable_kcp = false;
				break;
			}
			case 4:
			case 's':
			{
				sscanf(optarg, "%f", &frame_scale);
				break;
			}
		}
	}

	if(frame_scale < 0.1f)
		frame_scale = 0.1f;

	if(frame_scale > 1.0f)
		frame_scale = 1.0f;

	ret = server_net_init("0.0.0.0", 9999, enable_kcp);
	if(ret != 0)
	{
		log_error("server_net_init fail.");
		exit(-1);
	}
	ret = server_net_bind_pkg_cb(on_server_pkt);
	if(ret != 0)
	{
		log_error("server_net_bind_pkg_cb fail.");
		exit(-1);
	}
	ret = server_net_bind_connect_cb(on_client_connect);
	if(ret != 0)
	{
		log_error("server_net_bind_connect_cb fail.");
		exit(-1);
	}
	
	ret = pthread_create(&bradcast_thread, NULL, bradcast_video_thread, NULL);
	if(ret != 0)
	{
		log_error("pthread_create fail.");
		exit(-1);
	}

	enc_data = malloc(1024 * 1024 * 10);
	while(1)
	{
		if(server_start(capture, encodec) != 0)
			break;
		
		log_info("restart server.");
	}
	server_net_release();
}