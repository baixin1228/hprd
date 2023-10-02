#ifndef __ENCODEC_H__
#define __ENCODEC_H__

#include <glib.h>
#include "common.h"
#include "frame_buffer.h"

extern GSList *encodec_list;
#define ENCODEC_ADD_DEV(prio, dev)\
static __attribute__((constructor(prio + 100))) void __encodec_init__##prio()\
{\
	encodec_list = g_slist_append(encodec_list, &dev);\
}

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
	int (* map_fb)(struct encodec_object *obj, int buf_id);
	int (* put_fb)(struct encodec_object *obj, int buf_id);
	int (* get_fb)(struct encodec_object *obj);
	int (* unmap_fb)(struct encodec_object *obj, int buf_id);
	int (* force_i)(struct encodec_object *obj);
	int (* release)(struct encodec_object *obj);
};

struct encodec_object *encodec_init(struct mem_pool *pool, char *name);
int encodec_set_info(struct encodec_object *encodec_obj, GHashTable *fb_info);
int encodec_map_fb(struct encodec_object *encodec_obj, int buf_id);
int encodec_unmap_fb(struct encodec_object *encodec_obj, int buf_id);
int encodec_get_fb(struct encodec_object *encodec_obj);
int encodec_put_fb(struct encodec_object *encodec_obj, int buf_id);
int encodec_force_i(struct encodec_object *encodec_obj);
int encodec_regist_event_callback(struct encodec_object *obj,
	void ( *callback)(char *buf, size_t len));
int encodec_release(struct encodec_object *encodec_obj);

#endif