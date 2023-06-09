#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "util.h"
#include "codec.h"
#include "decodec.h"
#include "protocol.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "net/net_util.h"
#include "net/net_client.h"

struct request_obj
{
	uint32_t id;
	void *py_self;
	void (* callback)(void *oqu, uint32_t width, uint32_t height);
	struct request_obj *next;
};

struct client {
	char ip[24];
	uint16_t port;
	struct mem_pool client_pool;
	struct display_object *dsp_obj;
	struct decodec_object *dec_obj;
	void *resize_python_self;
	void (* resize_callback)(void *oqu, uint32_t width, uint32_t height);
	struct request_obj *req_obj_head;
	pthread_mutex_t req_tab_lock;
	struct data_queue recv_queue;
	atomic_int recv_pkt_count;
	struct data_pkt *recv_pkt;

	uint32_t frame_sum;
	uint32_t recv_sum;
} client = {0};

void add_request(struct request_obj *node)
{
	pthread_mutex_lock(&client.req_tab_lock);
	if(client.req_obj_head == NULL)
	{
		client.req_obj_head = node;
	}else{
		node->next = client.req_obj_head;
		client.req_obj_head = node;
	}
	pthread_mutex_unlock(&client.req_tab_lock);
}

struct request_obj *get_del_request(uint32_t id)
{
	pthread_mutex_lock(&client.req_tab_lock);
	struct request_obj *ret = client.req_obj_head;
	struct request_obj *last = ret;
	while(ret)
	{
		if(ret->id == id)
		{
			last->next = ret->next;
			break;
		}
		last = ret;
		ret = ret->next;
	}
	pthread_mutex_unlock(&client.req_tab_lock);
	return ret;
}

void _on_response(struct response_event *event)
{
	int id = ntohl(event->id);
	struct request_obj *req = get_del_request(id);
	if(!req)
	{
		log_error("[on_setting] not find set id:%d", id);
		return;
	}

	switch(event->ret)
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

uint32_t py_kcp_active() {
	return client_kcp_active();
}

uint32_t py_get_recv_count() {
	uint32_t ret = client.recv_sum;
	client.recv_sum = 0;
	return ret;
}

uint32_t py_get_queue_len() {
	return atomic_load(&client.recv_pkt_count);
}

static void _decode_pkt() {
	while(atomic_load(&client.recv_pkt_count) > 0)
	{
		dequeue_data(&client.recv_queue, (uint8_t *)client.recv_pkt, sizeof(* client.recv_pkt));
		client.recv_pkt->data_len = ntohl(client.recv_pkt->data_len);
		dequeue_data(&client.recv_queue, (uint8_t *)client.recv_pkt->data, client.recv_pkt->data_len);

		atomic_fetch_sub(&client.recv_pkt_count, 1);

		switch(client.recv_pkt->channel) {
			case VIDEO_CHANNEL: {
				_on_video(client.recv_pkt->data, client.recv_pkt->data_len);
				goto END;
			}
			default : {
				log_info("unknow channel.");
				break;
			}
		}
	}
END:
	return;
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
	static bool first = true;
	uint32_t frame_rate = 61;
	GHashTable *fb_info = g_hash_table_new(g_str_hash, g_str_equal);

	_decode_pkt();

	if(decodec_get_info(client.dec_obj, fb_info) == -1) {
		if(first)
			log_info("client waiting dec info.");
		first = false;
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

static void _on_client_recv(char *buf, size_t len) {
	int ret;
	struct data_pkt *recv_pkt = (struct data_pkt *)buf;

	switch(recv_pkt->channel) {
		case VIDEO_CHANNEL: {
			while((ret = enqueue_data(&client.recv_queue, buf, len)) == 0);
			if(ret == -1)
			{
				log_error("enqueue fail.");
				exit(-1);
			}
			atomic_fetch_add(&client.recv_pkt_count, 1);
			break;
		}
		case RESPONSE_CHANNEL: {
			_on_response((struct response_event *)recv_pkt->data);
			break;
		}
		default : {
			log_error("unknow channel.");
			break;
		}
	}

	client.recv_sum += len;
}

int py_client_connect(char *ip, uint16_t port) {
	int ret;
	int buf_id;

	debug_info_regist();

	strcpy(client.ip, ip);
	client.port = port;
	printf("connect\n");
	ret = client_net_init(ip, port);
	if (ret == -1) {
		printf("connect fail\n");
		return -1;
	}
	printf("connect success\n");
	client_net_bind_pkg_cb(_on_client_recv);
	
	pthread_mutex_init(&client.req_tab_lock, NULL);
	atomic_init(&client.recv_pkt_count, 0);

	client.recv_pkt = calloc(BUFLEN, 1);

	client.dec_obj = decodec_init(&client.client_pool, NULL);
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

	return 0;
}

int py_kcp_connect(uint32_t kcp_id) {
	int ret;
	ret = client_kcp_connect(client.ip, client.port, kcp_id);
	if (ret == -1) {
		log_error("kcp connect fail.");
	}
	return ret;
}

int py_client_resize(uint32_t width, uint32_t height) {
	if(!client.dsp_obj)
		return -1;

	return display_resize(client.dsp_obj, width, height);
}

int py_client_scale(float width, float height) {
	if(!client.dsp_obj)
		return -1;

	return display_scale(client.dsp_obj, width, height);
}

int py_client_regist_stream_size_cb(void *oqu, void (*callback)(void *oqu,
									uint32_t width, uint32_t height)) {
	client.resize_python_self = oqu;
	client.resize_callback = callback;
	return 0;
}

int py_client_close() {
	client_net_release();
	return 0;
}

int py_mouse_move(int x, int y) {
	struct input_event event;

	event.type = MOUSE_MOVE;
	event.x = x;
	event.y = y;

	if(send_input_event((char *)&event, sizeof(event))
			== -1) {
		log_error("send_input_event fail.");
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

	if(send_input_event((char *)&event, sizeof(event))
			== -1) {
		log_error("send_input_event fail.");
		return -1;
	}

	return 0;
}

int py_wheel_event(int key) {
	struct input_event event;

	event.type = MOUSE_WHEEL;
	event.key_code = key;

	if(send_input_event((char *)&event, sizeof(event))
			== -1) {
		log_error("send_input_event fail.");
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

	if(send_input_event((char *)&event, sizeof(event))
			== -1) {
		log_error("send_input_event fail.");
		return -1;
	}

	return 0;
}

int py_clip_event(char *type, char *data, uint16_t len) {
	if(send_clip_event(type, data, len) == -1) {
		log_error("send_clip_event fail.");
		return -1;
	}

	return 0;
}

int _send_request_data(void *oqu, uint32_t cmd, uint32_t value,
	void (*callback)(void *oqu, uint32_t ret, uint32_t value)) {
	static uint32_t id = 0;
	struct request_obj *set_req;

	if(oqu == NULL)
	{
		log_error("_send_request_data python self is none.");
		return -1;
	}
	if(callback == NULL)
	{
		log_error("_send_request_data callback is none.");
		return -1;
	}
	set_req = calloc(1, sizeof(*set_req));
	if(!set_req)
	{
		log_error("_send_request_data malloc fail.");
		return -1;
	}
	set_req->py_self = oqu;
	set_req->callback = callback;
	set_req->id = id;
	add_request(set_req);

	if(send_request_event(cmd, value, id) == -1) {
		log_error("send_request_event fail.");
		set_req = get_del_request(id);
		free(set_req);
		return -1;
	}
	id++;
	return 0;
}

int py_get_frame_rate(void *oqu, void (*callback)(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_request_data(oqu, GET_FRAME_RATE, 0, callback);
}

int py_get_bit_rate(void *oqu, void (*callback)(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_request_data(oqu, GET_BIT_RATE, 0, callback);
}

int py_set_frame_rate(void *oqu, uint32_t frame_rate, void (*callback)
	(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_request_data(oqu, SET_FRAME_RATE, frame_rate, callback);
}

int py_set_bit_rate(void *oqu, uint32_t bit_rate, void (*callback)
	(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_request_data(oqu, SET_BIT_RATE, bit_rate, callback);
}

int py_set_share_clipboard(void *oqu, uint32_t is_share, void (*callback)
	(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_request_data(oqu, SET_SHARE_CLIPBOARD, is_share, callback);
}

int py_get_remote_fps(void *oqu, void (*callback)
	(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_request_data(oqu, GET_FPS, 0, callback);
}

int py_get_client_id(void *oqu, void (*callback)
	(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_request_data(oqu, GET_CLIENT_ID, 0, callback);
}

int py_ping(void *oqu, uint32_t time, void (*callback)
	(void *oqu, uint32_t ret, uint32_t value)) {
	return _send_request_data(oqu, PING, time, callback);
}

void* alloc_mutex()
{
	pthread_mutex_t *mutex;
	mutex = malloc(sizeof(*mutex));
	if(mutex == NULL)
	{
		log_error("%s malloc fail.", __func__);
		return NULL;
	}
	pthread_mutex_init(mutex, NULL);
	return mutex;
}

void free_mutex(void* opaque)
{
	free(opaque);
}

void mutex_lock(void* opaque)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)opaque;
	pthread_mutex_lock(mutex);
}

void mutex_unlock(void* opaque)
{
	pthread_mutex_t *mutex = (pthread_mutex_t *)opaque;
	pthread_mutex_unlock(mutex);
}

typedef struct {
	pthread_spinlock_t lock;
} spinlock_proxy;

void* alloc_spinlock()
{
	spinlock_proxy *spinlock;
	spinlock = malloc(sizeof(*spinlock));
	if(spinlock == NULL)
	{
		log_error("%s malloc fail.", __func__);
		return NULL;
	}
	pthread_spin_init(&spinlock->lock, PTHREAD_PROCESS_SHARED);
	return spinlock;
}

void free_spinlock(void* opaque)
{
	free(opaque);
}

void spinlock_lock(void* opaque)
{
	spinlock_proxy *spinlock = (spinlock_proxy *)opaque;
	pthread_spin_lock(&spinlock->lock);
}

void spinlock_unlock(void* opaque)
{
	spinlock_proxy *spinlock = (spinlock_proxy *)opaque;
	pthread_spin_unlock(&spinlock->lock);
}