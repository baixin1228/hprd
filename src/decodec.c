#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "decodec.h"
#include "frame_buffer.h"
#include "dev_templete.h"

GSList *decodec_list = NULL;

static gint dev_comp (gconstpointer a, gconstpointer b)
{
	struct decodec_ops *dev_a = (struct decodec_ops *)a;
	char *str_b = (char *)b;
	return g_ascii_strcasecmp(dev_a->name, str_b);
}

struct decodec_object *decodec_init(struct mem_pool *pool, char *name)
{
	int ret;
	GSList *item = NULL;
	struct decodec_ops *dev_ops;
	struct decodec_object *dec_obj;

	dec_obj = calloc(1, sizeof(struct decodec_object));

	dec_obj->buf_pool = pool;

	if(!decodec_list)
	{
		log_error("can not find any decodec dev.");
		return NULL;
	}
	dev_ops = (struct decodec_ops *)decodec_list->data;

	if(name)
	{
		item = g_slist_find_custom(decodec_list, name, (GCompareFunc)dev_comp);
		if(!item)
		{
			log_warning("not find decodec dev:%s, use default:%s", name, 
				dev_ops->name);
		}else{
			dev_ops = (struct decodec_ops *)item->data;
		}
	}
	log_info("decodec dev is:%s", dev_ops->name);

	ret = dev_ops->init(dec_obj);
	if(ret == 0)
	{
		dec_obj->ops = dev_ops;
	}else{
		log_error("libffmpeg_dec.so init fail.");
		exit(-1);
	}
	
	return dec_obj;
}

int decodec_put_pkt(struct decodec_object *dec_obj, char *buf, size_t len)
{
	struct decodec_ops *dev_ops;

	if(!dec_obj)
		return -1;

	dev_ops = (struct decodec_ops *)dec_obj->ops;
	if(dev_ops)
	{
		if(dev_ops->put_pkt)
			return dev_ops->put_pkt(dec_obj, buf, len);
	}

	return -1;
}

DEV_GET_INFO(decodec, decodec_ops)
DEV_FUNC_2(decodec, map_fb)
DEV_FUNC_2(decodec, unmap_fb)
DEV_FUNC_1(decodec, get_fb)
DEV_FUNC_2(decodec, put_fb)
DEV_RELEASE(decodec, decodec_ops)