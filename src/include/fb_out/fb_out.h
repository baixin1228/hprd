#ifndef FB_OUT_H
#define FB_OUT_H
#include "buffer.h"
#include "module.h"

struct fb_out_ops
{
	char * name;
	int (* init)(struct module_data *dev);
	int (* put_buffer)(struct module_data *dev, struct common_buffer *buffer);
	int (* main_loop)(struct module_data *dev);
	int (* release)(struct module_data *dev);
};

#define FRAME_BUFFER_OUTPUT_DEV(dev, prio) REGISTE_MODULE_DEV(dev, FRAMEBUFFER_OUTPUT_DEV, prio)

struct module_data *init_frame_buffer_output_dev(void);
int put_frame_buffer(struct module_data *fb_out_data, struct common_buffer *buffer);
int fb_out_main_loop(struct module_data *fb_out_data);

#endif