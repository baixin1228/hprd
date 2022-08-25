#ifndef __FB_CONVERT_H__
#define __FB_CONVERT_H__
#include "buffer.h"
#include "module.h"

struct fb_convert_ops
{
	char * name;
	int (* init)(struct module_data *dev);
	int (* convert)(struct module_data *dev,
		struct common_buffer *src,
		struct common_buffer *dst);
	int (* release)(struct module_data *dev);
};

#define REGISTE_fb_convert_DEV(dev, prio) REGISTE_MODULE_DEV(dev, fb_convert_DEV, prio)

struct module_data *fc_init_dev();
int fc_convert(struct module_data *dev,
	struct common_buffer *src,
	struct common_buffer *dst);
int fc_release(struct module_data *dev);
#endif