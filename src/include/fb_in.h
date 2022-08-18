#ifndef __FB_IN_H__
#define __FB_IN_H__
#include "module.h"

struct fb_in_ops
{
	char * name;
	int (* init)(struct module_data *dev);
	int (* get_fb_info)(struct module_data *fb_in_data, struct frame_buffer_info *fb_info);
	struct common_buffer *(* get_data)(struct module_data *dev);
	int (* main_loop)(struct module_data *dev);
	int (* release)(struct module_data *dev);
};

#define FRAME_BUFFER_INPUT_DEV(dev, prio) REGISTE_MODULE_DEV(dev, FRAMEBUFFER_INPUT_DEV, prio)

struct module_data *fb_in_init_dev(void);
int fb_in_get_info(struct module_data *fb_in_data, struct frame_buffer_info *fb_info);
struct common_buffer *fb_in_get_fb(struct module_data *fb_in_data);
int fb_in_main_loop(struct module_data *fb_in_data);

#endif