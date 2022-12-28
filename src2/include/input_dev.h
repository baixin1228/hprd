#ifndef __INPUT_DEV_H__
#define __INPUT_DEV_H__

#include "frame_buffer.h"

struct input_dev_ops;

struct input_object
{
	void *priv;
	void (* on_event)(struct input_dev_ops *obj);
	struct input_dev_ops *ops;
};

struct input_dev_ops
{
	char * name;
	int (* init)(struct input_object *obj);
	int (* get_info)(struct input_object *obj, struct fb_info *info);
	int (* map_buffer)(struct input_object *obj, struct raw_buffer *buf);
	int (* put_buffer)(struct input_object *obj, struct raw_buffer *buf);
	struct raw_buffer *(* get_buffer)(struct input_object *obj);
	int (* unmap_buffer)(struct input_object *obj, struct raw_buffer *buf);
	int (* event_loop)(struct input_object *obj);
	int (* release)(struct input_object *obj);
};

struct input_object *input_dev_init(void);
int input_get_info(struct input_object *input_obj, struct fb_info *fb_info);
struct raw_buffer *input_get_fb(struct input_object *input_obj);
int input_main_loop(struct input_object *input_obj);

#endif