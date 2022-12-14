#ifndef __OUTPUT_DEV_H__
#define __OUTPUT_DEV_H__

#include "frame_buffer.h"

struct output_dev_ops;

struct output_objct
{
	void *priv;
	void (* on_event)(void);
	struct output_dev_ops *ops;
};

struct output_dev_ops
{
	char * name;
	int (* init)(struct output_objct *obj);
	int (* set_info)(struct output_objct *obj, struct fb_info *info);
	int (* map_buffer)(struct output_objct *obj, struct raw_buffer *buf);
	int (* put_buffer)(struct output_objct *obj, struct raw_buffer *buf);
	struct raw_buffer *(* get_buffer)(struct output_objct *obj);
	int (* unmap_buffer)(struct output_objct *obj, struct raw_buffer *buf);
	int (* event_loop)(struct output_objct *obj);
	int (* release)(struct output_objct *obj);
};

struct output_objct *output_dev_init(void);

#endif