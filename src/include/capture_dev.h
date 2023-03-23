#ifndef __CAPTURE_dev_H__
#define __CAPTURE_dev_H__

#include <glib.h>
#include "frame_buffer.h"

struct capture_dev_ops;

struct capture_object
{
	void *priv;
	struct mem_pool *buf_pool;
	void (* on_frame)(struct capture_object *obj);
	struct capture_dev_ops *ops;
};

struct capture_dev_ops
{
	char * name;
	int (* init)(struct capture_object *obj);
	int (* set_info)(struct capture_object *obj, GHashTable *info);
	int (* get_info)(struct capture_object *obj, GHashTable *info);
	int (* map_buffer)(struct capture_object *obj, int buf_id);
	int (* put_buffer)(struct capture_object *obj, int buf_id);
	int (* get_buffer)(struct capture_object *obj);
	int (* unmap_buffer)(struct capture_object *obj, int buf_id);
	int (* main_loop)(struct capture_object *obj);
	int (* change_frame_rate)(struct capture_object *obj, uint32_t frame_rate);
	int (* release)(struct capture_object *obj);
};

struct capture_object *capture_dev_init(struct mem_pool *pool);
int capture_get_info(struct capture_object *capture_obj, GHashTable *fb_info);
int capture_set_info(struct capture_object *capture_obj, GHashTable *fb_info);
int capture_map_fb(struct capture_object *capture_obj, int buf_id);
int capture_get_fb(struct capture_object *capture_obj);
int capture_put_fb(struct capture_object *capture_obj, int buf_id);
int capture_regist_event_callback(struct capture_object *capture_obj, void (* on_frame)(struct capture_object *obj));
int capture_main_loop(struct capture_object *capture_obj);
int capture_change_frame_rate(struct capture_object *capture_obj,
	uint32_t frate_rate);

#endif