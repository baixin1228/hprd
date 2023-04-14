#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "display_dev.h"
#include "frame_buffer.h"
#include "dev_templete.h"

extern struct display_ops sdl_ops;

struct display_object *display_dev_init(void)
{
	int ret;
	struct display_ops *dev_ops;
	struct display_object *display_obj;

	display_obj = calloc(1, sizeof(struct display_object));

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

DEV_SET_INFO(display, display_ops)
DEV_FUNC_2(display, map_fb)
DEV_FUNC_2(display, unmap_fb)
DEV_FUNC_1(display, get_fb)
DEV_FUNC_2(display, put_fb)
DEV_RELEASE(display, display_ops)

int display_regist_event_callback(struct display_object *display_obj, void (* on_frame)(struct display_object *obj))
{
	if(!display_obj)
		return -1;

	display_obj->on_frame = on_frame;

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