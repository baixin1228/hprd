#include <stdlib.h>

#include "util.h"
#include "buffer.h"
#include "fb_in/fb_in.h"

MODULE_BOUNDARY(null_func, FRAMEBUFFER_INPUT_DEV);

struct module_data *init_frame_buffer_input_dev(void)
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