#include <stdlib.h>

#include "util.h"
#include "module.h"
#include "fb_in.h"

void *null_func(void) {return NULL;}

MODULE_BOUNDARY(null_func, FRAMEBUFFER_INPUT_DEV);

int init_frame_buffer_input_dev()
{
	int ret;
	struct fb_in_ops *dev_ops;
	struct module_data *data = calloc(1, sizeof(struct module_data));

	const struct __module_item *item;
	FOREACH_ITEM(item, null_func, FRAMEBUFFER_INPUT_DEV)
	{
		dev_ops = item->handler();
		printf("init %s\n", dev_ops->name);
		ret = dev_ops->init(data);
		if(ret == 0)
		{
			data->ops = (void *)dev_ops;
			break;
		}
	}

	dev_ops = (struct fb_in_ops *)data->ops;
	for (int i = 0; i < 100; ++i)
	{
		dev_ops->get_data(data);
	}

	return 0;
}

int main()
{
	init_frame_buffer_input_dev();
	return 0;
}