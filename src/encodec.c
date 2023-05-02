#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "encodec.h"
#include "frame_buffer.h"
#include "dev_templete.h"

GSList *encodec_list = NULL;

static gint dev_comp (gconstpointer a, gconstpointer b)
{
	struct encodec_ops *dev_a = (struct encodec_ops *)a;
	char *str_b = (char *)b;
	return g_ascii_strcasecmp(dev_a->name, str_b);
}

struct encodec_object *encodec_init(struct mem_pool *pool, char *name)
{
	int ret;
	GSList *item = NULL;
	struct encodec_ops *dev_ops;
	struct encodec_object *enc_obj;

	enc_obj = calloc(1, sizeof(struct encodec_object));

	enc_obj->buf_pool = pool;

	if(!encodec_list)
	{
		log_error("can not find any encodec dev.");
		return NULL;
	}
	dev_ops = (struct encodec_ops *)encodec_list->data;

	if(name)
	{
		item = g_slist_find_custom(encodec_list, name, (GCompareFunc)dev_comp);
		if(!item)
		{
			log_warning("not find encodec dev:%s, use default:%s", name, 
				dev_ops->name);
		}else{
			dev_ops = (struct encodec_ops *)item->data;
		}
	}
	log_info("encodec dev is:%s", dev_ops->name);

	ret = dev_ops->init(enc_obj);
	if(ret == 0)
	{
		enc_obj->ops = dev_ops;
	}else{
		log_error("init encodec:%s fail.\n", name);
		exit(-1);
	}
	
	return enc_obj;
}

int encodec_regist_event_callback(struct encodec_object *enc_obj,
	void ( *callback)(char *buf, size_t len))
{
	if(!enc_obj)
		return -1;

	enc_obj->pkt_callback = callback;

	return 0;
}

DEV_SET_INFO(encodec, encodec_ops)
DEV_FUNC_2(encodec, map_fb)
DEV_FUNC_2(encodec, unmap_fb)
DEV_FUNC_1(encodec, get_fb)
DEV_FUNC_2(encodec, put_fb)
DEV_FUNC_1(encodec, force_i)
DEV_RELEASE(encodec, encodec_ops)