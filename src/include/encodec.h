#ifndef __ENCODEC_H__
#define __ENCODEC_H__
#include "buffer.h"
#include "module.h"
#include "codec.h"

struct encodec_info
{
	struct frame_buffer_info fb_info;
	enum STREAM_FORMAT stream_fmt;
	uint8_t quality;
};

struct encodec_ops
{
	char * name;
	int (* init)(struct module_data *encodec_dev, struct encodec_info enc_info);
	int (* push_fb)(struct module_data *dev, struct common_buffer *buffer);
	struct common_buffer *(* get_package)(struct module_data *dev);
	int (* release)(struct module_data *dev);
};

#define REGISTE_ENCODEC_DEV(dev, prio) REGISTE_MODULE_DEV(dev, ENCODEC_DEV, prio)

struct module_data *encodec_init_dev(struct encodec_info enc_info);
int encodec_push_fb(struct module_data *dev, struct common_buffer *buffer);
struct common_buffer *encodec_get_package(struct module_data *dev);
int encodec_release(struct module_data *dev);
#endif