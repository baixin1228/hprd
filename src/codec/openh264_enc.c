#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wels/codec_api.h>
#include <wels/codec_ver.h>

#include "util.h"
#include "codec.h"
#include "queue.h"
#include "encodec.h"
#include "buffer_pool.h"
#include "buffer_util.h"

struct openh264_enc_data {
	ISVCEncoder *encoder;
	SEncParamBase param;
	SSourcePicture pic;
	SFrameBSInfo info;
	uint32_t fb_format;
	uint32_t width;
	uint32_t hor_stride;
	uint32_t height;
	uint32_t ver_stride;

	struct int_queue id_queue;
};

static int openh264_enc_init(struct encodec_object *obj) {
	struct openh264_enc_data *enc_data;

	enc_data = calloc(1, sizeof(*enc_data));
	if(!enc_data) {
		log_error("calloc fail, check free memery.");
		return -1;
	}

	if (WelsCreateSVCEncoder (&enc_data->encoder)  ||
			(NULL == enc_data->encoder)) {
		printf ("Create encoder failed.\n");
		goto FAIL1;
	}

	obj->priv = enc_data;
	return 0;

FAIL1:
	free(enc_data);
	return -1;
}

static void _libopenh264_trace_callback(void *ctx, int level, const char *msg) {
	// log_info("[%p] %s", ctx, msg);
}

uint32_t com_to_svc_fmt(uint32_t format) {
	switch(format) {
		case ARGB8888:
		{
			return videoFormatRGBA;
		}
		case RGB888: {
			return videoFormatRGB;
		}
		case YUV420P: {
			return videoFormatI420;
		}
		case NV12: {
			return videoFormatNV12;
		}
	}

	return videoFormatI420;
}

static int openh264_enc_set_info(
	struct encodec_object *obj, GHashTable *enc_info) {
	uint32_t width;
	uint32_t height;
	uint32_t bit_rate;
	uint32_t frame_rate;
	uint32_t stream_format;
	WelsTraceCallback callback_function;
	struct openh264_enc_data *priv = obj->priv;

	if(g_hash_table_contains(enc_info, "frame_rate"))
		frame_rate = *(uint32_t *)g_hash_table_lookup(enc_info, "frame_rate");
	else
		frame_rate = 30;

	stream_format = com_to_svc_fmt(*(uint32_t *)g_hash_table_lookup(enc_info, "stream_fmt"));
	if(stream_format != STREAM_H264) {
		log_error("stream_format is invalid.");
		return -1;
	}

	bit_rate = *(uint32_t *)g_hash_table_lookup(enc_info, "bit_rate");
	width = *(uint32_t *)g_hash_table_lookup(enc_info, "width");
	height = *(uint32_t *)g_hash_table_lookup(enc_info, "height");
	priv->fb_format = com_to_svc_fmt(*(uint32_t *)g_hash_table_lookup(enc_info, "format"));

	priv->param.iUsageType     = SCREEN_CONTENT_REAL_TIME;
	priv->param.fMaxFrameRate  = frame_rate;
	priv->param.iPicWidth      = width;
	priv->param.iPicHeight     = height;
	priv->param.iTargetBitrate = bit_rate;

	// 日志级别
	uint32_t log_level = WELS_LOG_ERROR;
	(*priv->encoder)->SetOption(priv->encoder,
								DECODER_OPTION_TRACE_LEVEL, &log_level);
	callback_function = _libopenh264_trace_callback;
	(*priv->encoder)->SetOption(priv->encoder,
								DECODER_OPTION_TRACE_CALLBACK, (void *)&callback_function);
	(*priv->encoder)->SetOption(priv->encoder,
								DECODER_OPTION_TRACE_CALLBACK_CONTEXT, (void *)obj);

	// 初始化
	if (cmResultSuccess != (*priv->encoder)->Initialize(priv->encoder, &priv->param)) {
		log_error("Initialize fail.");
		return -1;
	}

	return 0;
}

static int openh264_enc_map_buf(struct encodec_object *obj, int buf_id) {
	return 0;
}

static int openh264_frame_enc(struct encodec_object *obj, int buf_id) {
	struct raw_buffer *buffer;
	struct openh264_enc_data *priv = obj->priv;

	buffer = get_raw_buffer(obj->buf_pool, buf_id);

	// 初始化图片对象
	memset(&priv->pic, 0, sizeof(SSourcePicture));
	priv->pic.iPicWidth = buffer->width;
	priv->pic.iPicHeight = buffer->height;

	priv->pic.iColorFormat = priv->fb_format;
	priv->pic.iStride[0] = buffer->hor_stride;
	priv->pic.iStride[1] = buffer->hor_stride;
	priv->pic.iStride[2] = buffer->hor_stride;
	priv->pic.pData[0] = (uint8_t *)buffer->ptrs[0];
	priv->pic.pData[1] = (uint8_t *)buffer->ptrs[1];
	priv->pic.pData[2] = (uint8_t *)buffer->ptrs[2];

	int rv = (*priv->encoder)->EncodeFrame (priv->encoder, &priv->pic, &priv->info);
	if (rv != cmResultSuccess) {
		log_error("encodec frame fail.");
		return -1;
	}
	if (priv->info.eFrameType != videoFrameTypeSkip) {
		for (int i = 0; i < priv->info.iLayerNum; ++i) {
			SLayerBSInfo layerInfo = priv->info.sLayerInfo[i];
			int layerSize = 0;
			for (int j = 0; j < layerInfo.iNalCount; ++j) {
				layerSize += layerInfo.pNalLengthInByte[j];
			}
			obj->pkt_callback((char *)layerInfo.pBsBuf, layerSize);
		}
	}

	while(queue_put_int(&priv->id_queue, buf_id) != 0)
		log_warning("queue_put_int fail.");

	return 0;
}

static int openh264_enc_getbuf(struct encodec_object *obj) {
	int ret;
	struct openh264_enc_data *priv = obj->priv;

	while((ret = queue_get_int(&priv->id_queue)) == -1)
		log_warning("queue_get_int fail.");

	return ret;
}

static int openh264_enc_release(struct encodec_object *obj) {
	struct openh264_enc_data *priv = obj->priv;
	WelsDestroySVCEncoder (priv->encoder);
	free(priv);
	return 0;
}

struct encodec_ops openh264_enc_ops = {
	.name               = "openh264_encodec",
	.init               = openh264_enc_init,
	.set_info           = openh264_enc_set_info,
	.map_buffer         = openh264_enc_map_buf,
	.put_buffer         = openh264_frame_enc,
	.get_buffer         = openh264_enc_getbuf,
	.release            = openh264_enc_release
};