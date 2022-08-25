#include <stdlib.h>

#include "util.h"
#include "buffer.h"
#include "decodec.h"

MODULE_BOUNDARY(null_func, DECODEC_DEV);

struct module_data *decodec_init_dev(struct decodec_info dec_info)
{
	int ret;
	struct decodec_ops *dev_ops;
	struct module_data *decodec_data;

	decodec_data = calloc(1, sizeof(struct module_data));

	const struct __module_item *item;
	FOREACH_ITEM(item, null_func, DECODEC_DEV)
	{
		dev_ops = item->handler();
		log_info("%s init...", dev_ops->name);
		ret = dev_ops->init(decodec_data, dec_info);
		if(ret == 0)
		{
			decodec_data->ops = (void *)dev_ops;
			break;
		}
	}

	if(decodec_data->ops == 0)
	{
		free(decodec_data);
		return NULL;
	}else
		return decodec_data;
}

enum PUSH_STATUS decodec_push_pkt(struct module_data *decodec_data, struct common_buffer *buffer)
{
	struct decodec_ops *dev_ops;
	
	if(!decodec_data)
		return PUSH_UNKNOW;

	dev_ops = (struct decodec_ops *)decodec_data->ops;
	if(dev_ops)
	{
		if(dev_ops->push_pkt)
			return dev_ops->push_pkt(decodec_data, buffer);
	}

	return PUSH_UNKNOW;
}

struct common_buffer *decodec_get_fb(struct module_data *decodec_data)
{
	struct decodec_ops *dev_ops;
	
	if(!decodec_data)
		return NULL;

	dev_ops = (struct decodec_ops *)decodec_data->ops;
	if(dev_ops)
	{
		if(dev_ops->get_fb)
			return dev_ops->get_fb(decodec_data);
	}

	return NULL;
}

int decodec_release(struct module_data *decodec_data)
{
	struct decodec_ops *dev_ops;
	
	if(!decodec_data)
		return -1;

	dev_ops = (struct decodec_ops *)decodec_data->ops;
	if(dev_ops)
	{
		if(dev_ops->release)
			return dev_ops->release(decodec_data);
	}

	return -1;
}