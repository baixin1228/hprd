#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include "util.h"
#include "encodec.h"
#include "frame_convert.h"

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

static int sw_scale_init(struct module_data *sw_scale_dev)
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

static void _full_sw_info(struct common_buffer *buf, char *datas[3], int *linesize)
{
	switch(buf->format)
	{
		case RGB444:
			linesize[0] = buf->hor_stride * buf->bpp / 8;
			linesize[1] = 0;
			linesize[2] = 0;
			linesize[3] = 0;
			datas[0] = buf->ptr;
		break;
		case RGB888:
			linesize[0] = buf->hor_stride * buf->bpp / 8;
			linesize[1] = 0;
			linesize[2] = 0;
			linesize[3] = 0;
			datas[0] = buf->ptr;
		break;
		case YUV420P:
			linesize[0] = buf->hor_stride;
			linesize[1] = buf->hor_stride / 2;
			linesize[2] = buf->hor_stride / 2;
			linesize[3] = 0;
			datas[0] = buf->ptr;
			datas[1] = buf->ptr + buf->hor_stride;
			datas[2] = buf->ptr + buf->hor_stride + buf->hor_stride / 4;
		break;
		case NV12:
			linesize[0] = buf->hor_stride;
			linesize[1] = buf->hor_stride;
			linesize[2] = 0;
			linesize[3] = 0;
			datas[0] = buf->ptr;
			datas[1] = buf->ptr + buf->hor_stride;
		break;
		default:
			linesize[0] = buf->hor_stride;
			linesize[1] = buf->hor_stride / 2;
			linesize[2] = buf->hor_stride / 2;
			linesize[3] = 0;
			datas[0] = buf->ptr;
			datas[1] = buf->ptr + buf->hor_stride;
			datas[2] = buf->ptr + buf->hor_stride + buf->hor_stride / 4;
		break;
	}
}

static  int sw_scale_convert(struct module_data *sw_scale_dev, struct common_buffer *src, struct common_buffer *dst)
{
	bool update_sw = false;
	int src_linesize[4] = {0};
	int dst_linesize[4] = {0};
	char *src_data[4] = {0};
	char *dst_data[4] = {0};

	struct sw_scale_data *sw_data = sw_scale_dev->priv;
	if(!sw_data->sw_ctx)
	{
		memcpy(&sw_data->src, src, sizeof(struct common_buffer));
		memcpy(&sw_data->dst, dst, sizeof(struct common_buffer));
		_update_sw_ctx(sw_data);
	}else{
		if(sw_data->src.width != src->width
			|| sw_data->src.height != src->height
			|| sw_data->src.format != src->format)
		{
			memcpy(&sw_data->src, src, sizeof(struct common_buffer));
			update_sw = true;
		}
		if(sw_data->dst.width != dst->width
			|| sw_data->dst.height != dst->height
			|| sw_data->dst.format != dst->format)
		{
			memcpy(&sw_data->dst, dst, sizeof(struct common_buffer));
			update_sw = true;
		}
		if(update_sw)
		{
			_update_sw_ctx(sw_data);
		}
	}

	_full_sw_info(src, src_data, src_linesize);
	_full_sw_info(dst, dst_data, dst_linesize);


	// AVFrame *rgbFrame = av_frame_alloc();
	// #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	// avpicture_fill((AVPicture *)rgbFrame, (uint8_t *)src->ptr, AV_PIX_FMT_BGR24, src->width, src->height);

	// log_info("%p %p %p %p %p %p", rgbFrame->data[0],
	// 	rgbFrame->data[1],
	// 	rgbFrame->data[2],
	// 	src_data[0],
	// 	src_data[1],
	// 	src_data[2]
	// 	);

	// log_info("%d %d %d %d %d %d", rgbFrame->linesize[0],
	// 	rgbFrame->linesize[1],
	// 	rgbFrame->linesize[2],
	// 	src_linesize[0],
	// 	src_linesize[1],
	// 	src_linesize[2]
	// 	);

    sws_scale(sw_data->sw_ctx,
    	(const unsigned char * const*)src_data,
    	src_linesize,
    	0,
    	src->height,
    	(uint8_t * const*)dst_data,
    	dst_linesize);

	return 0;
}

static int sw_scale_release(struct module_data *sw_scale_dev)
{
	struct sw_scale_data *sw_data = sw_scale_dev->priv;

	sws_freeContext(sw_data->sw_ctx);
	sw_data->sw_ctx = NULL;
	return 0;
}

struct frame_convert_ops sw_scale_dev = 
{
	.name               = "sw_scale_dev",
	.init               = sw_scale_init,
	.convert 	        = sw_scale_convert,
	.release            = sw_scale_release
};

REGISTE_FRAME_CONVERT_DEV(sw_scale_dev, DEVICE_PRIO_LOW);