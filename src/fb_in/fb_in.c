#include <stdlib.h>

#include "util.h"
#include "buffer.h"
#include "fb_in.h"

MODULE_BOUNDARY(null_func, FRAMEBUFFER_INPUT_DEV);

struct module_data *fb_in_init_dev(void)
{
	int ret;
	struct fb_in_ops *dev_ops;
	struct module_data *fb_in_data;

	fb_in_data = calloc(1, sizeof(struct module_data));

	const struct __module_item *item;
	FOREACH_ITEM(item, null_func, FRAMEBUFFER_INPUT_DEV)
	{
		dev_ops = item->handler();
		log_info("%s init...", dev_ops->name);
		ret = dev_ops->init(fb_in_data);
		if(ret == 0)
		{
			fb_in_data->ops = (void *)dev_ops;
			break;
		}
	}

	if(fb_in_data->ops == 0)
	{
		free(fb_in_data);
		return NULL;
	}else
		return fb_in_data;
}


int fb_in_get_info(struct module_data *fb_in_data, struct frame_buffer_info *fb_info)
{
	struct fb_in_ops *dev_ops;

	if(!fb_in_data)
		return -1;

	dev_ops = (struct fb_in_ops *)fb_in_data->ops;
	if(dev_ops)
	{
		if(dev_ops->get_fb_info)
			return dev_ops->get_fb_info(fb_in_data, fb_info);
	}

	return -1;
}

struct common_buffer *fb_in_get_fb(struct module_data *fb_in_data)
{
	struct fb_in_ops *dev_ops;

	if(!fb_in_data)
		return NULL;

	dev_ops = (struct fb_in_ops *)fb_in_data->ops;
	if(dev_ops)
	{
		if(dev_ops->get_data)
			return dev_ops->get_data(fb_in_data);
	}

	return NULL;
}

int fb_in_main_loop(struct module_data *fb_in_data)
{
	struct fb_in_ops *dev_ops;

	if(!fb_in_data)
		return -1;

	dev_ops = (struct fb_in_ops *)fb_in_data->ops;
	if(dev_ops)
	{
		if(dev_ops->main_loop)
			return dev_ops->main_loop(fb_in_data);
	}

	return -1;
}