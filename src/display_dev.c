#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "frame_buffer.h"
#include "dev_templete.h"

GSList *display_list = NULL;

static gint dev_comp (gconstpointer a, gconstpointer b)
{
	struct display_ops *dev_a = (struct display_ops *)a;
	char *str_b = (char *)b;
	return g_ascii_strcasecmp(dev_a->name, str_b);
}

struct display_object *display_dev_init(struct mem_pool *pool, char *name)
{
	int ret;
	GSList *item = NULL;
	struct display_ops *dev_ops;
	struct display_object *display_obj;

	display_obj = calloc(1, sizeof(struct display_object));

	display_obj->buf_pool = pool;

	if(!display_list)
	{
		log_error("can not find any display dev.");
		return NULL;
	}
	dev_ops = (struct display_ops *)display_list->data;

	if(name)
	{
		item = g_slist_find_custom(display_list, name, (GCompareFunc)dev_comp);
		if(!item)
		{
			log_warning("not find display dev:%s, use default:%s", name, 
				dev_ops->name);
		}else{
			dev_ops = (struct display_ops *)item->data;
		}
	}
	log_info("display dev is:%s", dev_ops->name);

	ret = dev_ops->init(display_obj);
	if(ret == 0)
	{
		display_obj->ops = dev_ops;
	}else{
		log_error("libsdl_display.so init fail.");
		exit(-1);
	}

	return display_obj;
}

DEV_SET_INFO(display, display_ops)
DEV_FUNC_2(display, map_fb)
DEV_FUNC_2(display, unmap_fb)
DEV_FUNC_1(display, get_fb)
DEV_FUNC_2(display, put_fb)
DEV_RESIZE(display, display_ops)
DEV_RELEASE(display, display_ops)

int display_regist_frame_callback(struct display_object *display_obj, void (* on_frame)(struct display_object *obj))
{
	if(!display_obj)
		return -1;

	display_obj->on_frame = on_frame;

	return 0;
}

int display_regist_event_callback(struct display_object *display_obj,
	void (* on_event)(struct display_object *obj, struct input_event *event))
{
	if(!display_obj)
		return -1;

	display_obj->on_event = on_event;

	return 0;
}

int display_main_loop(struct display_object *display_obj)
{
	struct display_ops *dev_ops;

	if(!display_obj)
		return -1;

	dev_ops = (struct display_ops *)display_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->main_loop)
			return dev_ops->main_loop(display_obj);
		else
			printf("display dev not find func:main_loop\n");
	}

	return -1;
}