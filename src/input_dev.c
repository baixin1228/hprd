#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "input_dev.h"
#include "buffer_pool.h"
#include "frame_buffer.h"
#include "dev_templete.h"

extern struct input_ops xlib_input_ops;

GSList *input_list = NULL;

static gint dev_comp (gconstpointer a, gconstpointer b)
{
	struct input_ops *dev_a = (struct input_ops *)a;
	char *str_b = (char *)b;
	return g_ascii_strcasecmp(dev_a->name, str_b);
}

struct input_object *input_init(char *name)
{
	int ret;
	GSList *item = NULL;
	struct input_ops *dev_ops;
	struct input_object *input_obj;

	input_obj = calloc(1, sizeof(struct input_object));

	if(!input_list)
	{
		log_error("can not find any input dev.");
		return NULL;
	}
	dev_ops = (struct input_ops *)input_list->data;

	if(name)
	{
		item = g_slist_find_custom(input_list, name, (GCompareFunc)dev_comp);
		if(!item)
		{
			log_warning("not find input dev:%s, use default:%s", name, 
				dev_ops->name);
		}else{
			dev_ops = (struct input_ops *)item->data;
		}
	}
	log_info("input dev is:%s", dev_ops->name);

	ret = dev_ops->init(input_obj);
	if(ret == 0)
	{
		input_obj->ops = dev_ops;
	}else{
		log_error("xlib_input_ops init fail.");
		exit(-1);
	}

	return input_obj;
}

DEV_SET_INFO(input, input_ops)
DEV_RELEASE(input, input_ops)

int input_push_key(struct input_object *obj, struct input_event *event)
{
	struct input_ops *dev_ops;

	if(!obj)
		return -1;

	dev_ops = (struct input_ops *)obj->ops;
	if(dev_ops)
	{
		if(dev_ops->push_key)
			return dev_ops->push_key(obj, event);
		else
			printf("input dev not find func:push_key\n");
	}

	return -1;
}

int input_push_clip(struct input_object *obj, struct clip_event *event)
{
	struct input_ops *dev_ops;

	if(!obj)
		return -1;

	dev_ops = (struct input_ops *)obj->ops;
	if(dev_ops)
	{
		if(dev_ops->push_clip)
			return dev_ops->push_clip(obj, event);
		else
			printf("input dev not find func:push_clip\n");
	}

	return -1;
}