#ifndef __OUTPUT_DEV_H__
#define __OUTPUT_DEV_H__

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
	int (* set_info)(struct output_object *obj, struct fb_info *info);
	int (* map_buffer)(struct output_object *obj, struct raw_buffer *buf);
	int (* put_buffer)(struct output_object *obj, struct raw_buffer *buf);
	struct raw_buffer *(* get_buffer)(struct output_object *obj);
	int (* unmap_buffer)(struct output_object *obj, struct raw_buffer *buf);
	int (* event_loop)(struct output_object *obj);
	int (* release)(struct output_object *obj);
};

struct output_object *output_dev_init(void);
int output_set_info(struct output_object *output_obj, struct fb_info *fb_info);
int output_put_fb(struct output_object *output_obj, struct raw_buffer *buffer);
int output_regist_event_callback(struct output_object *output_obj,
	void (* on_event)(struct output_object *obj));

int output_main_loop(struct output_object *output_obj);

#endif