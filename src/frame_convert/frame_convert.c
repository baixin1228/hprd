#include <stdlib.h>

#include "util.h"
#include "buffer.h"
#include "frame_convert.h"

MODULE_BOUNDARY(null_func, FRAME_CONVERT_DEV);

struct module_data *frame_convert_init_dev(struct frame_convert_info enc_info)
{
	int ret;
	struct frame_convert_ops *dev_ops;
	struct module_data *frame_convert_data;

	frame_convert_data = calloc(1, sizeof(struct module_data));

	const struct __module_item *item;
	FOREACH_ITEM(item, null_func, FRAME_CONVERT_DEV)
	{
		dev_ops = item->handler();
		log_info("%s init...", dev_ops->name);
		ret = dev_ops->init(frame_convert_data, enc_info);
		if(ret == 0)
		{
			frame_convert_data->ops = (void *)dev_ops;
			break;
		}
	}

	if(frame_convert_data->ops == 0)
	{
		free(frame_convert_data);
		return NULL;
	}else
		return frame_convert_data;
}

int fc_set_dst_buf(struct module_data *frame_convert_data, struct common_buffer buffer)
{
	struct frame_convert_ops *dev_ops;
	
	if(!frame_convert_data)
		return -1;

	dev_ops = (struct frame_convert_ops *)frame_convert_data->ops;
	if(dev_ops)
	{
		if(dev_ops->push_fb)
			return dev_ops->set_dst_buf(frame_convert_data, buffer);
	}

	return -1;
}

int fc_set_src_buf(struct module_data *frame_convert_data, struct common_buffer buffer)
{
	struct frame_convert_ops *dev_ops;
	
	if(!frame_convert_data)
		return -1;

	dev_ops = (struct frame_convert_ops *)frame_convert_data->ops;
	if(dev_ops)
	{
		if(dev_ops->push_fb)
			return dev_ops->set_src_buf(frame_convert_data, buffer);
	}

	return -1;
}

int frame_convert_release(struct module_data *frame_convert_data)
{
	struct frame_convert_ops *dev_ops;
	
	if(!frame_convert_data)
		return -1;

	dev_ops = (struct frame_convert_ops *)frame_convert_data->ops;
	if(dev_ops)
	{
		if(dev_ops->release)
			return dev_ops->release(frame_convert_data);
	}

	return -1;
}