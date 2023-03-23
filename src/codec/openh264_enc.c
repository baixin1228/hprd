#include <stdio.h>
#include <stdatomic.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>

#include "util.h"
#include "queue.h"
#include "encodec.h"
#include "buffer_pool.h"
#include "ffmpeg_util.h"

struct openh264_enc_data {
	ISVCEncoder *encoder;
	SEncParamExt encoder_param;
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

uint32_t com_to_svc_fmt(uint32_t format) {
	switch(format) {
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
	int ret;
	uint32_t width;
	uint32_t height;
	uint32_t bit_rate;
	uint32_t frame_rate;
	uint32_t stream_format;
	struct openh264_enc_data *priv = obj->priv;

	if(g_hash_table_contains(enc_info, "frame_rate"))
		frame_rate = *(uint32_t *)g_hash_table_lookup(enc_info, "frame_rate");
	else
		frame_rate = 30;

	stream_format = com_to_svc_fmt(*(uint32_t *)g_hash_table_lookup(enc_info, "stream_fmt"));

	bit_rate = *(uint32_t *)g_hash_table_lookup(enc_info, "bit_rate");
	width = *(uint32_t *)g_hash_table_lookup(enc_info, "width");
	height = *(uint32_t *)g_hash_table_lookup(enc_info, "height");
	priv->fb_format = com_to_svc_fmt(*(uint32_t *)g_hash_table_lookup(enc_info, "format"));

	SEncParamExt param = &priv->encoder_param;
	param->iUsageType     = fb_fmt;
	param->fMaxFrameRate  = frame_rate;
	param->iPicWidth      = width;
	param->iPicHeight     = height;
	param->iTargetBitrate = bit_rate;
	return encoder->Initialize (param);

	// 日志级别
	log_level = WELS_LOG_ERROR;
	(*priv->encoder)->SetOption(priv->encoder,
								EnCODER_OPTION_TRACE_LEVEL, &log_level);
	callback_function = _libopenh264_trace_callback;
	(*priv->encoder)->SetOption(priv->encoder,
								EnCODER_OPTION_TRACE_CALLBACK, (void *)&callback_function);
	(*priv->encoder)->SetOption(priv->encoder,
								EnCODER_OPTION_TRACE_CALLBACK_CONTEXT, (void *)obj);

	// 初始化
	if (cmResultSuccess != (err = priv->encoder->Initialize(&priv->encoder_param))) {
		log_error("InitializeExt fail.");
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
	priv->pic.pData[0] = buffer->ptrs[0];
	priv->pic.pData[1] = buffer->ptrs[1];
	priv->pic.pData[2] = buffer->ptrs[2];

	int rv = priv->encoder->EncodeFrame (&priv->pic, &priv->info);
	ASSERT_TRUE (rv == cmResultSuccess);
	if (priv->info.eFrameType != videoFrameTypeSkip) {
		for (int i = 0; i < priv->info.iLayerNum; ++i) {
			const SLayerBSInfo &layerInfo = priv->info.sLayerInfo[i];
			int layerSize = 0;
			for (int j = 0; j < layerInfo.iNalCount; ++j) {
				layerSize += layerInfo.pNalLengthInByte[j];
			}
			obj->pkt_callback(layerInfo.pBsBuf, layerSize);
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