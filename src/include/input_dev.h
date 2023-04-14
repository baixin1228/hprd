#ifndef __INPUT_DEV_H__
#define __INPUT_DEV_H__

#include <glib.h>
#include "protocol.h"
#include "buffer_pool.h"

struct input_ops;

struct input_object
{
	void *priv;
	struct input_ops *ops;
};

struct input_ops
{
	char * name;
	uint32_t priority;
	int (* init)(struct input_object *obj);
	int (* set_info)(struct input_object *obj, GHashTable *info);
	int (* push_key)(struct input_object *obj, struct input_event *event);
	int (* release)(struct input_object *obj);
};

struct input_object *input_init();
int input_set_info(struct input_object *input_obj, GHashTable *fb_info);
int input_push_key(struct input_object *input_obj, struct input_event *event);
int input_release(struct input_object *input_obj);

#endif