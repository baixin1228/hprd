#include <stdlib.h>

#include "util.h"
#include "dl_help.h"
#include "output_dev.h"


struct output_objct *output_dev_init(void)
{
	int ret;
	struct output_dev_ops *dev_ops;
	struct output_objct *output_obj;

	output_obj = calloc(1, sizeof(struct output_objct));

	dev_ops = (struct output_dev_ops *)load_lib_data("output_dev/sdl_output/libsdl_output.so", "dev_ops");

	if(!dev_ops)
	{
		log_error("load libsdl_output.so fail.");
		exit(-1);
	}

	ret = dev_ops->init(output_obj);
	if(ret == 0)
	{
		output_obj->ops = dev_ops;
	}else{
		log_error("libsdl_output.so init fail.");
		exit(-1);
	}

	return output_obj;
}

int output_set_info(struct output_objct *output_obj, struct frame_buffer_info fb_info)
{
	struct output_dev_ops *dev_ops;
	
	if(!output_obj)
		return -1;

	dev_ops = (struct output_dev_ops *)output_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->set_fb_info)
			return dev_ops->set_fb_info(output_obj, fb_info);
	}

	return -1;
}

int output_put_fb(struct output_objct *output_obj, struct common_buffer *buffer)
{
	struct output_dev_ops *dev_ops;
	
	if(!output_obj)
		return -1;

	dev_ops = (struct output_dev_ops *)output_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->put_buffer)
			return dev_ops->put_buffer(output_obj, buffer);
	}

	return -1;
}

int output_main_loop(struct output_objct *output_obj)
{
	struct output_dev_ops *dev_ops;

	if(!output_obj)
		return -1;

	dev_ops = (struct output_dev_ops *)output_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->main_loop)
			return dev_ops->main_loop(output_obj);
	}

	return -1;
}