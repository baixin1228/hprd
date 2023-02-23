#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "encodec.h"
#include "frame_buffer.h"
#include "dev_templete.h"

extern struct encodec_ops ffmpeg_enc_ops;

struct encodec_object *encodec_init(struct mem_pool *pool)
{
	int ret;
	struct encodec_ops *dev_ops;
	struct encodec_object *enc_obj;

	enc_obj = calloc(1, sizeof(struct encodec_object));

	enc_obj->buf_pool = pool;

	dev_ops = &ffmpeg_enc_ops;

	if(!dev_ops)
	{
		char path_tmp[255];
		getcwd(path_tmp, 255);
		log_error("load libffmpegenc.so fail. dir:%s\n", path_tmp);
		exit(-1);
	}

	ret = dev_ops->init(enc_obj);
	if(ret == 0)
	{
		enc_obj->ops = dev_ops;
	}else{
		log_error("libffmpegenc.so init fail.");
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