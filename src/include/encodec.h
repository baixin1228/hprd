#ifndef __ENCODEC_H__
#define __ENCODEC_H__

#include <glib.h>
#include "frame_buffer.h"

struct encodec_ops;

struct encodec_object
{
	void *priv;
	struct mem_pool *buf_pool;
	void ( *pkt_callback)(char *buf, size_t len);
	struct encodec_ops *ops;
};

struct encodec_ops
{
	char *name;
	uint32_t priority;
	int (* init)(struct encodec_object *obj);
	int (* set_info)(struct encodec_object *obj, GHashTable *info);
	int (* map_buffer)(struct encodec_object *obj, int buf_id);
	int (* put_buffer)(struct encodec_object *obj, int buf_id);
	int (* get_buffer)(struct encodec_object *obj);
	int (* unmap_buffer)(struct encodec_object *obj, int buf_id);
	int (* release)(struct encodec_object *obj);
};

struct encodec_object *encodec_init(struct mem_pool *pool);
int encodec_set_info(struct encodec_object *display_obj, GHashTable *fb_info);
int encodec_map_fb(struct encodec_object *encodec_obj, int buf_id);
int encodec_get_fb(struct encodec_object *encodec_obj);
int encodec_put_fb(struct encodec_object *encodec_obj, int buf_id);
int encodec_regist_event_callback(struct encodec_object *obj,
	void ( *callback)(char *buf, size_t len));
int encodec_release(struct encodec_object *encodec_obj);

#endif