#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "encodec.h"
#include "frame_buffer.h"
#include "dev_templete.h"

extern struct encodec_ops ffmpeg_enc_ops;
extern struct encodec_ops openh264_enc_ops;

static struct encodec_ops* encodec_devs[] = {
	&ffmpeg_enc_ops,
	&openh264_enc_ops
};

struct encodec_object *encodec_init(struct mem_pool *pool, char *encodec_name)
{
	int ret;
	struct encodec_ops *dev_ops;
	struct encodec_object *enc_obj;

	enc_obj = calloc(1, sizeof(struct encodec_object));

	enc_obj->buf_pool = pool;

	dev_ops = GET_DEV_OPS(encodec_ops, encodec_devs, encodec_name);

	if(!dev_ops)
	{
		log_error("load encodec:%s fail.\n", encodec_name);
		exit(-1);
	}

	ret = dev_ops->init(enc_obj);
	if(ret == 0)
	{
		enc_obj->ops = dev_ops;
	}else{
		log_error("init encodec:%s fail.\n", encodec_name);
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
DEV_MAP_FB(encodec, encodec_ops)
DEV_UNMAP_FB(encodec, encodec_ops)
DEV_GET_FB(encodec, encodec_ops)
DEV_PUT_FB(encodec, encodec_ops)
DEV_RELEASE(encodec, encodec_ops)