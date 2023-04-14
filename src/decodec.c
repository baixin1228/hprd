#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "decodec.h"
#include "frame_buffer.h"
#include "dev_templete.h"

extern struct decodec_ops openh264_dec_ops;

struct decodec_object *decodec_init(struct mem_pool *pool)
{
	int ret;
	struct decodec_ops *dev_ops;
	struct decodec_object *dec_obj;

	dec_obj = calloc(1, sizeof(struct decodec_object));

	dec_obj->buf_pool = pool;

	dev_ops = &openh264_dec_ops;

	if(!dev_ops)
	{
		char path_tmp[255];
		getcwd(path_tmp, 255);
		log_error("load libffmpeg_dec.so fail. dir:%s\n", path_tmp);
		exit(-1);
	}

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