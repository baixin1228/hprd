#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "capture_dev.h"
#include "frame_buffer.h"
#include "dev_templete.h"

GSList *capture_list = NULL;

static gint dev_comp (gconstpointer a, gconstpointer b)
{
	struct capture_ops *dev_a = (struct capture_ops *)a;
	char *str_b = (char *)b;
	return g_ascii_strcasecmp(dev_a->name, str_b);
}

struct capture_object *capture_dev_init(struct mem_pool *pool, char *name)
{
	int ret;
	GSList *item = NULL;
	struct capture_ops *dev_ops;
	struct capture_object *capture_obj;

	capture_obj = calloc(1, sizeof(struct capture_object));

	capture_obj->buf_pool = pool;

	if(!capture_list)
	{
		log_error("can not find any capture dev.");
		return NULL;
	}
	dev_ops = (struct capture_ops *)capture_list->data;

	if(name)
	{
		item = g_slist_find_custom(capture_list, name, (GCompareFunc)dev_comp);
		if(!item)
		{
			log_warning("not find capture dev:%s, use default:%s", name, 
				dev_ops->name);
		}else{
			dev_ops = (struct capture_ops *)item->data;
		}
	}
	log_info("capture dev is:%s", dev_ops->name);

	ret = dev_ops->init(capture_obj);
	if(ret == 0)
	{
		capture_obj->ops = dev_ops;
	}else{
		log_error("xcb_capture.so init fail.");
		exit(-1);
	}
	
	return capture_obj;
}

DEV_SET_INFO(capture, capture_ops)
DEV_GET_INFO(capture, capture_ops)
DEV_FUNC_2(capture, map_fb)
DEV_FUNC_2(capture, unmap_fb)
DEV_FUNC_1(capture, get_fb)
DEV_FUNC_2(capture, put_fb)
DEV_RELEASE(capture, capture_ops)

int capture_regist_event_callback(struct capture_object *capture_obj, void (* on_frame)(struct capture_object *obj))
{
	if(!capture_obj)
		return -1;

	capture_obj->on_frame = on_frame;

	return 0;
}

int capture_get_fps(struct capture_object *capture_obj)
{
	if(!capture_obj)
		return -1;

	return capture_obj->fps;
}

int capture_main_loop(struct capture_object *capture_obj)
{
	struct capture_ops *dev_ops;

	if(!capture_obj)
		return -1;

	dev_ops = (struct capture_ops *)capture_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->main_loop)
			return dev_ops->main_loop(capture_obj);
	}

	return -1;
}

int capture_quit(struct capture_object *capture_obj)
{
	struct capture_ops *dev_ops;

	if(!capture_obj)
		return -1;

	dev_ops = (struct capture_ops *)capture_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->quit)
			return dev_ops->quit(capture_obj);
	}

	return -1;
}