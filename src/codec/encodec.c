#include <stdlib.h>

#include "util.h"
#include "buffer.h"
#include "codec/encodec.h"

MODULE_BOUNDARY(null_func, ENCODEC_DEV);

struct module_data *encodec_init_dev(void)
{
	int ret;
	struct encodec_ops *dev_ops;
	struct module_data *encodec_data;

	encodec_data = calloc(1, sizeof(struct module_data));

	const struct __module_item *item;
	FOREACH_ITEM(item, null_func, ENCODEC_DEV)
	{
		dev_ops = item->handler();
		log_info("%s init...\n", dev_ops->name);
		ret = dev_ops->init(encodec_data);
		if(ret == 0)
		{
			encodec_data->ops = (void *)dev_ops;
			break;
		}
	}

	if(encodec_data->ops == 0)
	{
		free(encodec_data);
		return NULL;
	}else
		return encodec_data;
}

int encodec_push_fb(struct module_data *encodec_data, struct common_buffer *buffer)
{
	struct encodec_ops *dev_ops;
	
	if(!encodec_data)
		return -1;

	dev_ops = (struct encodec_ops *)encodec_data->ops;
	if(dev_ops)
	{
		if(dev_ops->push_fb)
			return dev_ops->push_fb(encodec_data, buffer);
	}

	return -1;
}

struct common_buffer *encodec_get_package(struct module_data *encodec_data)
{
	struct encodec_ops *dev_ops;
	
	if(!encodec_data)
		return NULL;

	dev_ops = (struct encodec_ops *)encodec_data->ops;
	if(dev_ops)
	{
		if(dev_ops->get_package)
			return dev_ops->get_package(encodec_data);
	}

	return NULL;
}

int encodec_release(struct module_data *encodec_data)
{
	struct encodec_ops *dev_ops;
	
	if(!encodec_data)
		return -1;

	dev_ops = (struct encodec_ops *)encodec_data->ops;
	if(dev_ops)
	{
		if(dev_ops->release)
			return dev_ops->release(encodec_data);
	}

	return -1;
}