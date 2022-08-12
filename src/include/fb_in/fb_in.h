#ifndef FB_IN_H
#define FB_IN_H
#include "module.h"

struct fb_in_ops
{
	char * name;
	int (* init)(struct module_data *dev);
	struct common_buffer *(* get_data)(struct module_data *dev);
	int (* main_loop)(struct module_data *dev);
	int (* release)(struct module_data *dev);
};

#define FRAME_BUFFER_INPUT_DEV(dev, prio) REGISTE_MODULE_DEV(dev, FRAMEBUFFER_INPUT_DEV, prio)

struct module_data *init_frame_buffer_input_dev(void);
struct common_buffer *get_frame_buffer(struct module_data *fb_in_data);
int fb_in_main_loop(struct module_data *fb_in_data);

#endif