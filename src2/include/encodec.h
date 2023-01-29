#ifndef __ENCODEC_H__
#define __ENCODEC_H__

#include "frame_buffer.h"

struct encodec_ops;

struct encodec_object
{
	void *priv;
	struct encodec_ops *ops;
};

struct encodec_ops
{
	char * name;
	int (* init)(struct encodec_object *obj);
	int (* set_info)(struct encodec_object *obj, struct fb_info *info);
	int (* map_buffer)(struct encodec_object *obj, int buf_id);
	int (* put_buffer)(struct encodec_object *obj, int buf_id);
	int (* get_buffer)(struct encodec_object *obj);
	int (* unmap_buffer)(struct encodec_object *obj, int buf_id);
	int (* release)(struct encodec_object *obj);
};

struct encodec_object *encodec_init(void);
int output_set_info(struct encodec_object *output_obj, struct fb_info *fb_info);
int encodec_map_fb(struct encodec_object *encodec_obj, int buf_id);
int encodec_get_fb(struct encodec_object *encodec_obj);
int encodec_put_fb(struct encodec_object *encodec_obj, int buf_id);

#endif