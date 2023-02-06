#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "decodec.h"
#include "frame_buffer.h"
#include "dev_templete.h"

struct decodec_object *decodec_init(void)
{
	int ret;
	struct decodec_ops *dev_ops;
	struct decodec_object *obj;

	obj = calloc(1, sizeof(struct decodec_object));

	dev_ops = (struct decodec_ops *)load_lib_data(
		"src2/codec/ffmpeg/libffmpeg_dec.so", "dev_ops");

	if(!dev_ops)
	{
		char path_tmp[255];
		getcwd(path_tmp, 255);
		log_error("load libffmpeg_dec.so fail. dir:%s\n", path_tmp);
		exit(-1);
	}

	ret = dev_ops->init(obj);
	if(ret == 0)
	{
		obj->ops = dev_ops;
	}else{
		log_error("libffmpeg_dec.so init fail.");
		exit(-1);
	}
	
	return obj;
}

int decodec_put_pkt(struct decodec_object *obj, char *buf, size_t len)
{
	struct decodec_ops *dev_ops;

	if(!obj)
		return -1;

	dev_ops = (struct decodec_ops *)obj->ops;
	if(dev_ops)
	{
		if(dev_ops->put_pkt)
			return dev_ops->put_pkt(obj, buf, len);
	}

	return -1;
}

DEV_GET_INFO(decodec, decodec_ops)
DEV_MAP_FB(decodec, decodec_ops)
DEV_UNMAP_FB(decodec, decodec_ops)
DEV_GET_FB(decodec, decodec_ops)
DEV_PUT_FB(decodec, decodec_ops)
DEV_RELEASE(decodec, decodec_ops)