#ifndef __ENCODEC_H__
#define __ENCODEC_H__

#include <glib.h>
#include "frame_buffer.h"

struct encodec_ops;

struct encodec_object
{
	void *priv;
	void ( *pkt_callback)(char *buf, size_t len);
	struct encodec_ops *ops;
};

struct encodec_ops
{
	char *name;
	int (* init)(struct encodec_object *obj);
	int (* set_info)(struct encodec_object *obj, GHashTable *info);
	int (* map_buffer)(struct encodec_object *obj, int buf_id);
	int (* put_buffer)(struct encodec_object *obj, int buf_id);
	int (* get_buffer)(struct encodec_object *obj);
	int (* regist_pkt_callback)(struct encodec_object *obj,
								void ( *callback)(char *buf, size_t len));
	int (* unmap_buffer)(struct encodec_object *obj, int buf_id);
	int (* release)(struct encodec_object *obj);
};

struct encodec_object *encodec_init(void);
int encodec_set_info(struct encodec_object *output_obj, GHashTable *fb_info);
int encodec_map_fb(struct encodec_object *encodec_obj, int buf_id);
int encodec_get_fb(struct encodec_object *encodec_obj);
int encodec_put_fb(struct encodec_object *encodec_obj, int buf_id);

#endif