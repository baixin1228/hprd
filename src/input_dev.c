#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "input_dev.h"
#include "buffer_pool.h"
#include "frame_buffer.h"
#include "dev_templete.h"

extern struct input_dev_ops xlib_input_dev_ops;

struct input_object *input_dev_init(struct mem_pool *pool)
{
	int ret;
	struct input_dev_ops *dev_ops;
	struct input_object *input_obj;

	input_obj = calloc(1, sizeof(struct input_object));

	dev_ops = &xlib_input_dev_ops;

	if(!dev_ops)
	{
		char path_tmp[255];
		getcwd(path_tmp, 255);
		log_error("load xlib_input_dev_ops fail. dir:%s\n", path_tmp);
		exit(-1);
	}

	ret = dev_ops->init(input_obj);
	if(ret == 0)
	{
		input_obj->ops = dev_ops;
	}else{
		log_error("xlib_input_dev_ops init fail.");
		exit(-1);
	}

	return input_obj;
}

DEV_SET_INFO(input, input_dev_ops)
DEV_RELEASE(input, input_dev_ops)