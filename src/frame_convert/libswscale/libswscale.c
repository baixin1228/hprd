#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include "util.h"
#include "encodec.h"

#define PKT_BUFFER_SIZE (10 * 1024 * 1024)

struct sw_scale_data{
	struct SwsContext *sw_ctx;
	struct common_buffer dst;
	struct common_buffer src;
};

static int _com_fb_fmt_to_av_fmt(enum COMMON_BUFFER_FORMAT format)
{
	switch(format)
	{
		case RGB444:
			return AV_PIX_FMT_RGB444LE;
		break;
		case RGB888:
			return AV_PIX_FMT_RGB24;
		break;
		case YUV420P:
			return AV_PIX_FMT_YUV420P;
		break;
		case NV12:
			return AV_PIX_FMT_NV12;
		break;
		default:
			return AV_PIX_FMT_RGB24;
		break;
	}
}

static int sw_scale_init(struct module_data *sw_scale_dev, struct encodec_info enc_info)
{
	struct sw_scale_data *sw_data;

	sw_data = calloc(1, sizeof(*sw_data));
	if(!sw_data)
	{
		func_error("calloc fail, check free memery.");
		goto FAIL1;
	}
		 
	sw_scale_dev->priv = sw_data;
	return 0;

FAIL1:
	return -1;
}

static int _update_sw_ctx(struct sw_scale_data *sw_data)
{
	if(sw_data->sw_ctx)
		sws_freeContext(sw_data->sw_ctx);

	struct common_buffer *src = &sw_data->src;
	struct common_buffer *dst = &sw_data->dst;
	sw_data->sw_ctx = sws_getContext(
		src->width,
		src->height,
		_com_fb_fmt_to_av_fmt(src->format),
		dst->width,
		dst->height,
		_com_fb_fmt_to_av_fmt(dst->format),
		SWS_POINT,
		NULL,
		NULL,
		NULL);

	return 0;
}

static  int sw_scale_convert(struct module_data *sw_scale_dev, struct common_buffer *src, struct common_buffer *dst)
{
	bool update_sw = false;
	struct sw_scale_data *sw_data = sw_scale_dev->priv;

	if(!sw_data->sw_ctx)
	{
		memcpy(sw_data->src, src, sizeof(struct common_buffer));
		memcpy(sw_data->dst, dst, sizeof(struct common_buffer));
		_update_sw_ctx(sw_data);
	}else{
		if(sw_data->src.width != src->width
			|| sw_data->src.height != src->height
			|| sw_data->src.format != src->format)
		{
			memcpy(sw_data->src, src, sizeof(struct common_buffer));
			update_sw = true;
		}
		if(sw_data->dst.width != dst->width
			|| sw_data->dst.height != dst->height
			|| sw_data->dst.format != dst->format)
		{
			memcpy(sw_data->dst, dst, sizeof(struct common_buffer));
			update_sw = true;
		}
		if(update_sw)
		{
			_update_sw_ctx(sw_data);
		}
	}

    sws_scale(sw_data->sw_ctx,
    	src->ptr,
    	get_line_size(src),
    	0,
    	src->height,
    	dst->ptr,
    	get_line_size(dst));

	return 0;
}

static int sw_scale_release(struct module_data *sw_scale_dev)
{
	struct sw_scale_data *sw_data = sw_scale_dev->priv;

	sws_freeContext(sw_data->sw_ctx);
	sw_data->sw_ctx = NULL;
	return 0;
}

struct encodec_ops sw_scale_dev = 
{
	.name               = "sw_scale_dev",
	.init               = sw_scale_init,
	.convert 	        = sw_scale_convert,
	.release            = sw_scale_release
};

REGISTE_FRAME_CONVERT_DEV(sw_scale_dev, DEVICE_PRIO_LOW);