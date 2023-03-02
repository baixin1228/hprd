#ifndef __CAPTURE_dev_H__
#define __CAPTURE_dev_H__

#include <glib.h>
#include "frame_buffer.h"

struct capture_dev_ops;

struct input_object
{
	void *priv;
	struct mem_pool *buf_pool;
	void (* on_event)(struct input_object *obj);
	struct capture_dev_ops *ops;
};

struct capture_dev_ops
{
	char * name;
	int (* init)(struct input_object *obj);
	int (* set_info)(struct input_object *obj, GHashTable *info);
	int (* get_info)(struct input_object *obj, GHashTable *info);
	int (* map_buffer)(struct input_object *obj, int buf_id);
	int (* put_buffer)(struct input_object *obj, int buf_id);
	int (* get_buffer)(struct input_object *obj);
	int (* unmap_buffer)(struct input_object *obj, int buf_id);
	int (* event_loop)(struct input_object *obj);
	int (* release)(struct input_object *obj);
};

struct input_object *capture_dev_init(struct mem_pool *pool);
int input_get_info(struct input_object *input_obj, GHashTable *fb_info);
int input_set_info(struct input_object *input_obj, GHashTable *fb_info);
int input_map_fb(struct input_object *input_obj, int buf_id);
int input_get_fb(struct input_object *input_obj);
int input_put_fb(struct input_object *input_obj, int buf_id);
int input_regist_event_callback(struct input_object *input_obj, void (* on_event)(struct input_object *obj));
int input_main_loop(struct input_object *input_obj);

#endif