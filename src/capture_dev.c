#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "capture_dev.h"
#include "frame_buffer.h"
#include "dev_templete.h"

extern struct capture_dev_ops xcb_dev_ops;

struct capture_object *capture_dev_init(struct mem_pool *pool)
{
	int ret;
	struct capture_dev_ops *dev_ops;
	struct capture_object *capture_obj;

	capture_obj = calloc(1, sizeof(struct capture_object));

	capture_obj->buf_pool = pool;

	dev_ops = &xcb_dev_ops;

	if(!dev_ops)
	{
		char path_tmp[255];
		getcwd(path_tmp, 255);
		log_error("load xcb_capture.so fail. dir:%s\n", path_tmp);
		exit(-1);
	}

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

DEV_SET_INFO(capture, capture_dev_ops)
DEV_GET_INFO(capture, capture_dev_ops)
DEV_MAP_FB(capture, capture_dev_ops)
DEV_UNMAP_FB(capture, capture_dev_ops)
DEV_GET_FB(capture, capture_dev_ops)
DEV_PUT_FB(capture, capture_dev_ops)
DEV_RELEASE(capture, capture_dev_ops)

int capture_regist_event_callback(struct capture_object *capture_obj, void (* on_event)(struct capture_object *obj))
{
	if(!capture_obj)
		return -1;

	capture_obj->on_event = on_event;

	return 0;
}

int capture_main_loop(struct capture_object *capture_obj)
{
	struct capture_dev_ops *dev_ops;

	if(!capture_obj)
		return -1;

	dev_ops = (struct capture_dev_ops *)capture_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->event_loop)
			return dev_ops->event_loop(capture_obj);
	}

	return -1;
}