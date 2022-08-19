#ifndef __FRAME_CONVERT_H__
#define __FRAME_CONVERT_H__
#include "buffer.h"
#include "module.h"

struct frame_convert_ops
{
	char * name;
	int (* init)(struct module_data *dev);
	int (* convert)(struct module_data *dev,
		struct common_buffer *src,
		struct common_buffer *dst);
	int (* release)(struct module_data *dev);
};

#define REGISTE_FRAME_CONVERT_DEV(dev, prio) REGISTE_MODULE_DEV(dev, FRAME_CONVERT_DEV, prio)

struct module_data *fc_init_dev();
int fc_convert(struct module_data *dev,
	struct common_buffer *src,
	struct common_buffer *dst);
int fc_release(struct module_data *dev);
#endif