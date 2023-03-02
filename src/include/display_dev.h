#ifndef __DISPLAY_DEV_H__
#define __DISPLAY_DEV_H__

#include <glib.h>
#include "frame_buffer.h"

struct display_dev_ops;

struct display_object
{
	void *priv;
	struct mem_pool *buf_pool;
	void (* on_event)(struct display_object *obj);
	struct display_dev_ops *ops;
};

struct display_dev_ops
{
	char * name;
	int (* init)(struct display_object *obj);
	int (* set_info)(struct display_object *obj, GHashTable *info);
	int (* map_buffer)(struct display_object *obj, int buf_id);
	int (* put_buffer)(struct display_object *obj, int buf_id);
	int (* get_buffer)(struct display_object *obj);
	int (* unmap_buffer)(struct display_object *obj, int buf_id);
	int (* event_loop)(struct display_object *obj);
	int (* release)(struct display_object *obj);
};

struct display_object *display_dev_init(struct mem_pool *pool);
int display_set_info(struct display_object *display_obj, GHashTable *fb_info);
int display_map_fb(struct display_object *display_obj, int buf_id);
int display_get_fb(struct display_object *display_obj);
int display_put_fb(struct display_object *display_obj, int buf_id);
int display_regist_event_callback(struct display_object *display_obj,
	void (* on_event)(struct display_object *obj));

int display_main_loop(struct display_object *display_obj);

#endif