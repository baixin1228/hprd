#include <stdlib.h>

#include "util.h"
#include "fb_out/fb_out.h"

MODULE_BOUNDARY(null_func, FRAMEBUFFER_OUTPUT_DEV);

struct module_data *fb_out_init_dev(void)
{
	int ret;
	struct fb_out_ops *dev_ops;
	struct module_data *fb_out_data;

	fb_out_data = calloc(1, sizeof(struct module_data));

	const struct __module_item *item;
	FOREACH_ITEM(item, null_func, FRAMEBUFFER_OUTPUT_DEV)
	{
		dev_ops = item->handler();
		log_info("%s init...", dev_ops->name);
		ret = dev_ops->init(fb_out_data);
		if(ret == 0)
		{
			fb_out_data->ops = (void *)dev_ops;
			break;
		}
	}

	if(fb_out_data->ops == 0)
	{
		free(fb_out_data);
		return NULL;
	}else
		return fb_out_data;
}

int fb_out_set_info(struct module_data *fb_out_data, struct frame_buffer_info fb_info)
{
	struct fb_out_ops *dev_ops;
	
	if(!fb_out_data)
		return -1;

	dev_ops = (struct fb_out_ops *)fb_out_data->ops;
	if(dev_ops)
	{
		if(dev_ops->set_fb_info)
			return dev_ops->set_fb_info(fb_out_data, fb_info);
	}

	return -1;
}

int fb_out_put_fb(struct module_data *fb_out_data, struct common_buffer *buffer)
{
	struct fb_out_ops *dev_ops;
	
	if(!fb_out_data)
		return -1;

	dev_ops = (struct fb_out_ops *)fb_out_data->ops;
	if(dev_ops)
	{
		if(dev_ops->put_buffer)
			return dev_ops->put_buffer(fb_out_data, buffer);
	}

	return -1;
}

int fb_out_main_loop(struct module_data *fb_out_data)
{
	struct fb_out_ops *dev_ops;

	if(!fb_out_data)
		return -1;

	dev_ops = (struct fb_out_ops *)fb_out_data->ops;
	if(dev_ops)
	{
		if(dev_ops->main_loop)
			return dev_ops->main_loop(fb_out_data);
	}

	return -1;
}