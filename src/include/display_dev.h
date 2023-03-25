#ifndef __DISPLAY_DEV_H__
#define __DISPLAY_DEV_H__

#include <glib.h>
#include "frame_buffer.h"
#include "input_event.h"

struct display_dev_ops;

struct display_object
{
	void *priv;
	struct mem_pool *buf_pool;
	void (* on_event)(struct display_object *obj, struct input_event *event);
	void (* on_frame)(struct display_object *obj);
	struct display_dev_ops *ops;
};

struct display_dev_ops
{
	char * name;
	uint32_t priority;
	int (* init)(struct display_object *obj);
	int (* set_info)(struct display_object *obj, GHashTable *info);
	int (* map_buffer)(struct display_object *obj, int buf_id);
	int (* put_buffer)(struct display_object *obj, int buf_id);
	int (* get_buffer)(struct display_object *obj);
	int (* resize)(struct display_object *obj, uint32_t width, uint32_t height);
	int (* unmap_buffer)(struct display_object *obj, int buf_id);
	int (* main_loop)(struct display_object *obj);
	int (* release)(struct display_object *obj);
};

struct display_object *display_dev_init(struct mem_pool *pool,
	char *display_name);
int display_set_info(struct display_object *display_obj, GHashTable *fb_info);
int display_map_fb(struct display_object *display_obj, int buf_id);
int display_get_fb(struct display_object *display_obj);
int display_put_fb(struct display_object *display_obj, int buf_id);
int display_resize(struct display_object *display_obj, uint32_t width, uint32_t height);
int display_regist_frame_callback(struct display_object *display_obj,
	void (* on_frame)(struct display_object *obj));
int display_regist_event_callback(struct display_object *display_obj,
	void (* on_event)(struct display_object *obj, struct input_event *event));

int display_main_loop(struct display_object *display_obj);

#endif