#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "frame_buffer.h"
#include "dev_templete.h"

extern struct display_dev_ops sdl_ops;

struct display_object *display_dev_init(struct mem_pool *pool)
{
	int ret;
	struct display_dev_ops *dev_ops;
	struct display_object *display_obj;

	display_obj = calloc(1, sizeof(struct display_object));

	display_obj->buf_pool = pool;

	dev_ops = &sdl_ops;

	if(!dev_ops)
	{
		char path_tmp[255];
		getcwd(path_tmp, 255);
		log_error("load libsdl_display.so fail. dir:%s\n", path_tmp);
		exit(-1);
	}

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

DEV_SET_INFO(display, display_dev_ops)
DEV_MAP_FB(display, display_dev_ops)
DEV_UNMAP_FB(display, display_dev_ops)
DEV_GET_FB(display, display_dev_ops)
DEV_PUT_FB(display, display_dev_ops)
DEV_RELEASE(display, display_dev_ops)

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
	struct display_dev_ops *dev_ops;

	if(!display_obj)
		return -1;

	dev_ops = (struct display_dev_ops *)display_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->main_loop)
			return dev_ops->main_loop(display_obj);
		else
			printf("display dev not find func:main_loop\n");
	}

	return -1;
}