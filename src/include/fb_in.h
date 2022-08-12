#ifndef FRAME_BUFFER_INPUT_H
#define FRAME_BUFFER_INPUT_H
#include "module.h"

struct fb_in_ops
{
	char * name;
	int (* init)(struct module_data *dev);
	int (* get_data)(struct module_data *dev);
	int (* main_loop)(struct module_data *dev);
	int (* release)(struct module_data *dev);
};

#define FRAME_BUFFER_INPUT_DEV(dev, prio) REGISTE_MODULE_DEV(dev, FRAMEBUFFER_INPUT_DEV, prio)

#endif