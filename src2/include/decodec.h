#ifndef __DECODEC_H__
#define __DECODEC_H__

#include <glib.h>
#include "frame_buffer.h"

struct decodec_ops;

struct decodec_object
{
	void *priv;
	struct decodec_ops *ops;
};

struct decodec_ops
{
	char * name;
	int (* init)(struct decodec_object *obj);
	int (* get_info)(struct decodec_object *obj, GHashTable *info);
	int (* map_buffer)(struct decodec_object *obj, int buf_id);
	int (* put_buffer)(struct decodec_object *obj, int buf_id);
	int (* get_buffer)(struct decodec_object *obj);
	int (* unmap_buffer)(struct decodec_object *obj, int buf_id);
	int (* release)(struct decodec_object *obj);
};

struct decodec_object *decodec_init(void);
int decodec_get_info(struct decodec_object *decodec_obj,
	GHashTable *fb_info);
int decodec_map_fb(struct decodec_object *decodec_obj, int buf_id);
int decodec_get_fb(struct decodec_object *decodec_obj);
int decodec_put_fb(struct decodec_object *decodec_obj, int buf_id);

#endif