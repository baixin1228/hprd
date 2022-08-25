#include <stdlib.h>

#include "util.h"
#include "buffer.h"
#include "fb_convert.h"

MODULE_BOUNDARY(null_func, fb_convert_DEV);

struct module_data *fc_init_dev()
{
	int ret;
	struct fb_convert_ops *dev_ops;
	struct module_data *fb_convert_data;

	fb_convert_data = calloc(1, sizeof(struct module_data));

	const struct __module_item *item;
	FOREACH_ITEM(item, null_func, fb_convert_DEV)
	{
		dev_ops = item->handler();
		log_info("%s init...", dev_ops->name);
		ret = dev_ops->init(fb_convert_data);
		if(ret == 0)
		{
			fb_convert_data->ops = (void *)dev_ops;
			break;
		}
	}

	if(fb_convert_data->ops == 0)
	{
		free(fb_convert_data);
		return NULL;
	}else
		return fb_convert_data;
}

int fc_convert(struct module_data *fb_convert_data,
		struct common_buffer *src,
		struct common_buffer *dst)
{
	struct fb_convert_ops *dev_ops;
	
	if(!fb_convert_data)
		return -1;

	dev_ops = (struct fb_convert_ops *)fb_convert_data->ops;
	if(dev_ops)
	{
		if(dev_ops->convert)
		{
			dst->id = src->id;
			return dev_ops->convert(fb_convert_data, src, dst);
		}
	}

	return -1;
}

int fc_release(struct module_data *fb_convert_data)
{
	struct fb_convert_ops *dev_ops;
	
	if(!fb_convert_data)
		return -1;

	dev_ops = (struct fb_convert_ops *)fb_convert_data->ops;
	if(dev_ops)
	{
		if(dev_ops->release)
			return dev_ops->release(fb_convert_data);
	}

	return -1;
}