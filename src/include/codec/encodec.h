#ifndef __ENCODEC_H__
#define __ENCODEC_H__
#include "buffer.h"
#include "module.h"

struct encodec_ops
{
	char * name;
	int (* init)(struct module_data *dev);
	int (* push_fb)(struct module_data *dev, struct common_buffer *buffer);
	struct common_buffer *(* get_package)(struct module_data *dev);
	int (* release)(struct module_data *dev);
};

#define REGISTE_ENCODEC_DEV(dev, prio) REGISTE_MODULE_DEV(dev, ENCODEC_DEV, prio)

struct module_data *encodec_init_dev(void);
int encodec_push_fb(struct module_data *dev, struct common_buffer *buffer);
struct common_buffer *encodec_get_package(struct module_data *dev);
int encodec_release(struct module_data *dev);

#endif