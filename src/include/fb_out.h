#ifndef __FB_OUT_H__
#define __FB_OUT_H__
#include "buffer.h"
#include "module.h"

struct fb_out_ops
{
	char * name;
	int (* init)(struct module_data *dev);
	int (* set_fb_info)(struct module_data *dev, struct frame_buffer_info fb_info);
	int (* put_buffer)(struct module_data *dev, struct common_buffer *buffer);
	int (* main_loop)(struct module_data *dev);
	int (* release)(struct module_data *dev);
};

#define FRAME_BUFFER_OUTPUT_DEV(dev, prio) REGISTE_MODULE_DEV(dev, FRAMEBUFFER_OUTPUT_DEV, prio)

struct module_data *fb_out_init_dev(void);
int fb_out_set_info(struct module_data *fb_in_data, struct frame_buffer_info fb_info);
int fb_out_put_fb(struct module_data *fb_out_data, struct common_buffer *buffer);
int fb_out_main_loop(struct module_data *fb_out_data);

#endif