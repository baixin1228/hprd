#ifndef __DISPLAY_DEV_H__
#define __DISPLAY_DEV_H__

#include <glib.h>
#include "frame_buffer.h"
#include "protocol.h"

extern GSList *display_list;
#define DISPLAY_ADD_DEV(prio, dev)\
static __attribute__((constructor(prio + 100))) void __display_init__##prio()\
{\
	display_list = g_slist_append(display_list, &dev);\
}

struct display_ops;
struct display_object
{
	void *priv;
	struct mem_pool *buf_pool;
	void (* on_event)(struct display_object *obj, struct input_event *event);
	void (* on_frame)(struct display_object *obj);
	struct display_ops *ops;
};

struct display_ops
{
	char * name;
	uint32_t priority;
	int (* init)(struct display_object *obj);
	int (* set_info)(struct display_object *obj, GHashTable *info);
	int (* map_fb)(struct display_object *obj, int buf_id);
	int (* put_fb)(struct display_object *obj, int buf_id);
	int (* get_fb)(struct display_object *obj);
	int (* resize)(struct display_object *obj, uint32_t width, uint32_t height);
	int (* scale)(struct display_object *obj, float width, float height);
	int (* unmap_fb)(struct display_object *obj, int buf_id);
	int (* main_loop)(struct display_object *obj);
	int (* release)(struct display_object *obj);
};

struct display_object *display_dev_init(struct mem_pool *pool, char *name);
int display_set_info(struct display_object *display_obj, GHashTable *fb_info);
int display_map_fb(struct display_object *display_obj, int buf_id);
int display_get_fb(struct display_object *display_obj);
int display_put_fb(struct display_object *display_obj, int buf_id);
int display_resize(struct display_object *display_obj, uint32_t width, uint32_t height);
int display_scale(struct display_object *display_obj, float width, float height);
int display_regist_frame_callback(struct display_object *display_obj,
	void (* on_frame)(struct display_object *obj));
int display_regist_event_callback(struct display_object *display_obj,
	void (* on_event)(struct display_object *obj, struct input_event *event));

int display_main_loop(struct display_object *display_obj);

#endif