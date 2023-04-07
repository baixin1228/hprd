#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "util.h"
#include "codec.h"
#include "decodec.h"
#include "tcp_client.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "protocol.h"
#include "net_help.h"

struct setting_request
{
	uint32_t set_id;
	void *py_self;
	void (* callback)(void *oqu, uint32_t width, uint32_t height);
	struct setting_request *next;
};

struct client {
	int fd;
	struct mem_pool client_pool;
	struct display_object *dsp_obj;
	struct decodec_object *dec_obj;
	void *resize_python_self;
	void (* resize_callback)(void *oqu, uint32_t width, uint32_t height);
	struct setting_request *setting_head;
	pthread_mutex_t setting_lock;
	struct data_queue recv_queue;
	atomic_int recv_pkt_count;
	struct data_pkt *recv_pkt;

	uint32_t frame_sum;
	uint32_t recv_sum;
} client = {0};

void add_setting_request(struct setting_request *node)
{
	pthread_mutex_lock(&client.setting_lock);
	if(client.setting_head == NULL)
	{
		client.setting_head = node;
	}else{
		node->next = client.setting_head;
		client.setting_head = node;
	}
	pthread_mutex_unlock(&client.setting_lock);
}

struct setting_request *get_del_setting_request(uint32_t set_id)
{
	pthread_mutex_lock(&client.setting_lock);
	struct setting_request *ret = client.setting_head;
	struct setting_request *last = ret;
	while(ret)
	{
		if(ret->set_id == set_id)
		{
			last->next = ret->next;
			break;
		}
		last = ret;
		ret = ret->next;
	}
	pthread_mutex_unlock(&client.setting_lock);
	return ret;
}

void _on_setting(int fd, struct setting_event *event)
{
	int set_id = ntohl(event->set_id);
	struct setting_request *req = get_del_setting_request(set_id);
	if(!req)
	{
		log_error("[on_setting] not find set id:%d", set_id);
		return;
	}

	switch(event->cmd)
	{
		case RET_SUCCESS:
		{
			req->callback(req->py_self, 1, ntohl(event->value));
			break;
		}
		case RET_FAIL:
		{
			req->callback(req->py_self, 0, ntohl(event->value));
			break;
		}
	}
	free(req);
}

static void _on_video(char *buf, size_t len) {
	if(client.dec_obj) {
		decodec_put_pkt(client.dec_obj, buf, len);
	}
}

static void _decode_pkt() {
	if(atomic_load(&client.recv_pkt_count) == 0)
		return;

	pop_queue_data(&client.recv_queue, (uint8_t *)client.recv_pkt, sizeof(* client.recv_pkt));
	client.recv_pkt->data_len = ntohl(client.recv_pkt->data_len);
	pop_queue_data(&client.recv_queue, (uint8_t *)client.recv_pkt->data, client.recv_pkt->data_len);

	atomic_fetch_sub(&client.recv_pkt_count, 1);

	switch(client.recv_pkt->channel) {
		case VIDEO_CHANNEL: {
			_on_video(client.recv_pkt->data, client.recv_pkt->data_len);
			break;
		}
		case SETTING_CHANNEL: {
			_on_setting(client.fd, (struct setting_event *)client.recv_pkt->data);
			break;
		}
		default : {
			log_info("unknow channel.");
			break;
		}
	}
}

uint32_t py_get_and_clean_recv_sum() {
	uint32_t ret = client.recv_sum;
	client.recv_sum = 0;
	return ret;
}

int py_on_frame() {
	int ret;
	int buf_id;

	_decode_pkt();

	buf_id = decodec_get_fb(client.dec_obj);
	if(buf_id == -1) {
		return 0;
	}

	ret = display_put_fb(client.dsp_obj, buf_id);
	if(ret != 0) {
		log_error("display_put_fb fail.");
		return -1;
	}
	buf_id = display_get_fb(client.dsp_obj);
	if(buf_id == -1) {
		log_error("display_get_fb fail.");
		return -1;
	}

	ret = decodec_put_fb(client.dec_obj, buf_id);
	if(ret != 0) {
		log_error("decodec_put_fb fail.");
		return -1;
	}

	client.frame_sum++;

	return 0;
}

uint32_t py_get_and_clean_frame() {
	uint32_t ret = client.frame_sum;
	client.frame_sum = 0;
	return ret;
}

int py_client_init_config(uint64_t winid) {
	int ret = -2;
	uint32_t frame_rate = 61;
	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	_decode_pkt();

	if(decodec_get_info(client.dec_obj, fb_info) == -1) {
		log_info("client waiting dec info.");
		goto END;
	}

	g_hash_table_insert(fb_info, "frame_rate", &frame_rate);
	g_hash_table_insert(fb_info, "window", &winid);
	display_set_info(client.dsp_obj, fb_info);
	if(client.resize_callback != NULL)
		client.resize_callback(client.resize_python_self,
						   *(uint32_t *)g_hash_table_lookup(fb_info, "width"),
						   *(uint32_t *)g_hash_table_lookup(fb_info, "height"));
	ret = 0;
END:
	g_hash_table_destroy(fb_info);
	return ret;
}

static void _on_client_recv(int fd, char *buf, size_t len) {
	int ret;
	while((ret = queue_append_data(&client.recv_queue, buf, len)) == 0);
	if(ret == -1)
	{
		log_error("enqueue fail.");
		exit(-1);
	}
	client.recv_sum += len;
	atomic_fetch_add(&client.recv_pkt_count, 1);
}

#define BUFLEN 1024 * 1024 * 10
static char _recv_buf[BUFLEN];
void *recv_thread(void *oq) {
	while(tcp_recv_pkt(client.fd, _recv_buf, BUFLEN, _on_client_recv) != -1);
	log_warning("recv_thread exit.");
	return NULL;
}

int py_client_connect(char *ip, uint16_t port) {
	int ret;
	int buf_id;
	pthread_t p1;

	debug_info_regist();

	client.fd = client_connect(ip, port);

	if (client.fd == -1) {
		return -1;
	}

	pthread_mutex_init(&client.setting_lock, NULL);
	atomic_init(&client.recv_pkt_count, 0);

	client.recv_pkt = calloc(BUFLEN, 1);

	client.dec_obj = decodec_init(&client.client_pool);
	client.dsp_obj = display_dev_init(&client.client_pool, "x11_renderer");

	for (int i = 0; i < 5; ++i) {
		buf_id = alloc_buffer(&client.client_pool);
		if(buf_id == -1) {
			log_error("alloc_buffer fail.");
			return -1;
		}
		ret = decodec_map_fb(client.dec_obj, buf_id);
		if(ret != 0) {
			log_error("decodec_map_fb fail.");
			return -1;
		}
		ret = display_map_fb(client.dsp_obj, buf_id);
		if(ret != 0) {
			log_error("display_map_fb fail.");
			return -1;
		}
	}

	pthread_create(&p1, NULL, recv_thread, NULL);
	pthread_detach(p1);
	return 0;
}

int py_client_resize(uint32_t width, uint32_t height) {
	if(!client.dsp_obj)
		return -1;

	return display_resize(client.dsp_obj, width, height);
}

int py_client_regist_stream_size_cb(void *oqu, void (*callback)(void *oqu,
									uint32_t width, uint32_t height)) {
	client.resize_python_self = oqu;
	client.resize_callback = callback;
	return 0;
}

int py_client_close() {
	if(client.fd != -1) {
		close(client.fd);
		client.fd = -1;
	}

	return 0;
}

int py_mouse_move(int x, int y) {
	struct input_event event;

	event.type = MOUSE_MOVE;
	event.x = x;
	event.y = y;

	if(send_event(client.fd, INPUT_CHANNEL, (char *)&event, sizeof(event))
			== -1) {
		log_error("send_event fail.");
		return -1;
	}

	return 0;
}

int py_mouse_click(int x, int y, int key, int down_or_up) {
	struct input_event event;

	if(down_or_up) {
		event.type = MOUSE_DOWN;
	} else {
		event.type = MOUSE_UP;
	}

	event.key_code = key;
	event.x = x;
	event.y = y;

	if(send_event(client.fd, INPUT_CHANNEL, (char *)&event, sizeof(event))
			== -1) {
		log_error("send_event fail.");
		return -1;
	}

	return 0;
}

int py_wheel_event(int key) {
	struct input_event event;

	event.type = MOUSE_WHEEL;
	event.key_code = key;

	if(send_event(client.fd, INPUT_CHANNEL, (char *)&event, sizeof(event))
			== -1) {
		log_error("send_event fail.");
		return -1;
	}

	return 0;
}

int py_key_event(int key, int down_or_up) {
	struct input_event event;

	if(down_or_up) {
		event.type = KEY_DOWN;
	} else {
		event.type = KEY_UP;
	}

	event.key_code = key;

	if(send_event(client.fd, INPUT_CHANNEL, (char *)&event, sizeof(event))
			== -1) {
		log_error("send_event fail.");
		return -1;
	}

	return 0;
}

int _send_setting_data(void *oqu, uint32_t type, uint32_t frame_rate,
	void (*callback)(void *oqu, uint32_t ret, uint32_t value)) {
	static uint32_t set_id = 0;
	struct setting_event event;
	struct setting_request *set_req;

	if(oqu == NULL)
	{
		log_error("_send_setting_data python self is none.");
		return -1;
	}
	if(callback == NULL)
	{
		log_error("_send_setting_data callback is none.");
		return -1;
	}
	set_req = calloc(1, sizeof(*set_req));
	if(!set_req)
	{
		log_error("_send_setting_data malloc fail.");
		return -1;
	}
	set_req->py_self = oqu;
	set_req->callback = callback;
	set_req->set_id = set_id;
	add_setting_request(set_req);

	event.cmd = type;
	event.set_id = htonl(set_id);
	event.value = htonl(frame_rate);
	if(send_event(client.fd, SETTING_CHANNEL, (char *)&event, sizeof(event))
			== -1) {
		log_error("send_event fail.");
		set_req = get_del_setting_request(set_id);
		free(set_req);
		return -1;
	}
	set_id++;
	return 0;
}

int py_get_frame_rate(void *oqu, void (*callback)(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_setting_data(oqu, GET_FRAME_RATE, 0, callback);
}

int py_get_bit_rate(void *oqu, void (*callback)(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_setting_data(oqu, GET_BIT_RATE, 0, callback);
}

int py_set_frame_rate(void *oqu, uint32_t frame_rate, void (*callback)
	(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_setting_data(oqu, SET_FRAME_RATE, frame_rate, callback);
}

int py_set_bit_rate(void *oqu, uint32_t bit_rate, void (*callback)
	(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_setting_data(oqu, SET_BIT_RATE, bit_rate, callback);
}