#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <getopt.h>

#include "util.h"
#include "codec.h"
#include "decodec.h"
#include "protocol.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "net/net_client.h"

int fd;
struct decodec_object *dec_obj = NULL;

static void on_event(struct display_object *obj, struct input_event *event)
{
	if(send_input_event((char *)event, sizeof(struct input_event))
		== -1)
	{
		log_error("send_event fail.");
		exit(-1);
	}
}

static void on_package(char *buf, size_t len)
{
	if(dec_obj)
	{
		decodec_put_pkt(dec_obj, buf, len);
	}
}

static void display_on_frame(struct display_object *obj)
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
void client_main()
{
	int ret;
	int buf_id;
	uint32_t frame_rate = 61;
	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	dec_obj = decodec_init(&client_pool, NULL);
	out_obj = display_dev_init(&client_pool, "sdl_display");

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

	display_regist_frame_callback(out_obj, display_on_frame);
	display_regist_event_callback(out_obj, on_event);
	g_hash_table_insert(fb_info, "frame_rate", &frame_rate);
	display_set_info(out_obj, fb_info);
	
	g_hash_table_destroy(fb_info);

	display_main_loop(out_obj);
}

static void _on_client_pkt(char *buf, size_t len)
{
	struct data_pkt *pkt = (struct data_pkt *)buf;
	pkt->data_len = ntohl(pkt->data_len);

	switch(pkt->channel)
	{
		case VIDEO_CHANNEL:
		{
			on_package(pkt->data, pkt->data_len);
			break;
		}
		default: {
			break;
		}
	}
}

struct option long_options[] =
{
	{"help",  	no_argument,       NULL, 'h'},
	{"reqarg", 	required_argument, NULL, 'r'},
	{"optarg", 	optional_argument, NULL, 'o'},
	{"ip", 		required_argument, NULL, 'i'},
	{NULL,		0,                 NULL,  0}
};

void print_help()
{
	printf("help\n");
}

int main(int argc, char* argv[])
{
    int ret = -1;
    char *ip = NULL;
    int option_index = 0;

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
    		case 2:
    		case 3:
    		case 'i':
    		{
    			ip = optarg;
    			break;
    		}
    	}
        printf("ret = %c\t", ret);
        printf("optarg = %s\t", optarg);
        printf("optind = %d\t", optind);
        printf("argv[optind] = %s\t", argv[optind]);
        printf("option_index = %d\n", option_index);
    }
    if(ip == NULL)
    {
    	print_help();
		exit(0);
    }

	client_net_init(ip, 9999);
	client_net_bind_pkg_cb(_on_client_pkt);

	client_main();
}