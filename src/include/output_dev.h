#ifndef __OUTPUT_DEV_H__
#define __OUTPUT_DEV_H__

#include <glib.h>
#include "frame_buffer.h"

struct output_dev_ops;

struct output_object
{
	void *priv;
	void (* on_event)(struct output_object *obj);
	struct output_dev_ops *ops;
};

struct output_dev_ops
{
	char * name;
	int (* init)(struct output_object *obj);
	int (* set_info)(struct output_object *obj, GHashTable *info);
	int (* map_buffer)(struct output_object *obj, int buf_id);
	int (* put_buffer)(struct output_object *obj, int buf_id);
	int (* get_buffer)(struct output_object *obj);
	int (* unmap_buffer)(struct output_object *obj, int buf_id);
	int (* event_loop)(struct output_object *obj);
	int (* release)(struct output_object *obj);
};

struct output_object *output_dev_init(void);
int output_set_info(struct output_object *output_obj, GHashTable *fb_info);
int output_map_fb(struct output_object *output_obj, int buf_id);
int output_get_fb(struct output_object *output_obj);
int output_put_fb(struct output_object *output_obj, int buf_id);
int output_regist_event_callback(struct output_object *output_obj,
	void (* on_event)(struct output_object *obj));

int output_main_loop(struct output_object *output_obj);

#endif