#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wels/codec_api.h>
#include <wels/codec_ver.h>
#include <libswscale/swscale.h>

#include "util.h"
#include "codec.h"
#include "queue.h"
#include "encodec.h"
#include "buffer_pool.h"
#include "buffer_util.h"

struct openh264_enc_data {
	ISVCEncoder *encoder;
	SEncParamBase param;
	SEncParamExt paramExt;
	SSourcePicture pic;
	SFrameBSInfo info;
	uint32_t fb_format;
	uint32_t width;
	uint32_t hor_stride;
	uint32_t height;
	uint32_t ver_stride;
	uint64_t frame_count;
	uint16_t gop_size;
	uint16_t thread_count;

	struct SwsContext *sws_ctx;
	uint8_t * frame_data[4];
	int linesize[4];
	struct int_queue id_queue;
};

static int openh264_enc_init(struct encodec_object *obj) {
	struct openh264_enc_data *priv;

	priv = calloc(1, sizeof(*priv));
	if(!priv) {
		log_error("calloc fail, check free memery.");
		return -1;
	}

	if (WelsCreateSVCEncoder (&priv->encoder)  ||
			(NULL == priv->encoder)) {
		printf ("Create encoder failed.\n");
		goto FAIL1;
	}

	obj->priv = priv;
	return 0;

FAIL1:
	free(priv);
	return -1;
}

static void _libopenh264_trace_callback(void *ctx, int level, const char *msg) {
	log_info("openh264_enc [%p] %s", ctx, msg);
}

uint32_t com_to_svc_fmt(uint32_t format) {
	switch(format) {
	case ARGB8888: {
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

__attribute__((unused))
static int openh264_enc_set_info_simple(
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

	stream_format = *(uint32_t *)g_hash_table_lookup(enc_info, "stream_fmt");
	if(stream_format != STREAM_H264) {
		log_error("stream_format is invalid.%d %d", stream_format, STREAM_H264);
		return -1;
	}

	bit_rate = *(uint32_t *)g_hash_table_lookup(enc_info, "bit_rate");
	width = *(uint32_t *)g_hash_table_lookup(enc_info, "width");
	height = *(uint32_t *)g_hash_table_lookup(enc_info, "height");
	priv->fb_format = com_to_svc_fmt(*(uint32_t *)g_hash_table_lookup(enc_info, "format"));

	priv->sws_ctx = sws_getContext(
						width,
						height,
						AV_PIX_FMT_RGB32,
						width,
						height,
						AV_PIX_FMT_YUV420P,
						SWS_POINT, NULL, NULL, NULL);
	priv->linesize[0] = width;
	priv->linesize[1] = width / 2;
	priv->linesize[2] = width / 2;
	priv->linesize[3] = 0;
	priv->frame_data[0] = malloc(width * height * 4);
	priv->frame_data[1] = malloc(width * height);
	priv->frame_data[2] = malloc(width * height);
	priv->frame_data[3] = NULL;

	priv->param.iUsageType     = SCREEN_CONTENT_REAL_TIME;
	priv->param.fMaxFrameRate  = frame_rate;
	priv->param.iPicWidth      = width;
	priv->param.iPicHeight     = height;
	priv->param.iTargetBitrate = bit_rate;
	priv->param.iRCMode = RC_BITRATE_MODE;

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

	if(g_hash_table_contains(enc_info, "gop_size"))
		priv->gop_size = *(uint32_t *)g_hash_table_lookup(enc_info, "gop_size");
	else
		priv->gop_size = 60;

	if(g_hash_table_contains(enc_info, "thread_count"))
		priv->thread_count = *(uint32_t *)g_hash_table_lookup(enc_info, "thread_count");
	else
		priv->thread_count = 1;

	stream_format = *(uint32_t *)g_hash_table_lookup(enc_info, "stream_fmt");
	if(stream_format != STREAM_H264) {
		log_error("stream_format is invalid.%d %d", stream_format, STREAM_H264);
		return -1;
	}

	bit_rate = *(uint32_t *)g_hash_table_lookup(enc_info, "bit_rate");
	width = *(uint32_t *)g_hash_table_lookup(enc_info, "width");
	height = *(uint32_t *)g_hash_table_lookup(enc_info, "height");
	priv->fb_format = com_to_svc_fmt(*(uint32_t *)g_hash_table_lookup(enc_info, "format"));

	priv->sws_ctx = sws_getContext(
						width,
						height,
						AV_PIX_FMT_RGB32,
						width,
						height,
						AV_PIX_FMT_YUV420P,
						SWS_POINT, NULL, NULL, NULL);
	priv->linesize[0] = width;
	priv->linesize[1] = width / 2;
	priv->linesize[2] = width / 2;
	priv->linesize[3] = 0;
	priv->frame_data[0] = malloc(width * height * 4);
	priv->frame_data[1] = malloc(width * height);
	priv->frame_data[2] = malloc(width * height);
	priv->frame_data[3] = NULL;

	(*priv->encoder)->GetDefaultParams(priv->encoder, &priv->paramExt);

	priv->paramExt.iUsageType				= SCREEN_CONTENT_REAL_TIME;
	priv->paramExt.fMaxFrameRate			= frame_rate;
	priv->paramExt.iPicWidth				= width;
	priv->paramExt.iPicHeight				= height;
	priv->paramExt.iTargetBitrate			= bit_rate;
    priv->paramExt.iMaxBitrate				= bit_rate;
	priv->paramExt.iRCMode					= RC_BITRATE_MODE;
	priv->paramExt.iComplexityMode			= LOW_COMPLEXITY;
	priv->paramExt.uiIntraPeriod			= priv->gop_size;
    priv->paramExt.iMultipleThreadIdc		= priv->thread_count;
    priv->paramExt.iEntropyCodingModeFlag	= 0; /* CAVLC */
    priv->paramExt.bEnableFrameSkip			= 0;
    priv->paramExt.iNumRefFrame				= -1;
    priv->paramExt.bEnableSSEI				= 1;
    priv->paramExt.iMaxQp					= 51;
    priv->paramExt.iMinQp					= 24;
    priv->paramExt.bEnableAdaptiveQuant		= 1;
    priv->paramExt.iTemporalLayerNum		= 1;

	// 空间层
	SSpatialLayerConfig *spaticalLayerCfg = &(priv->paramExt.sSpatialLayers[SPATIAL_LAYER_0]);
	spaticalLayerCfg->iVideoWidth			= width;
	spaticalLayerCfg->iVideoHeight			= height;
	spaticalLayerCfg->fFrameRate			= frame_rate;
	spaticalLayerCfg->iSpatialBitrate		= priv->paramExt.iTargetBitrate;
	spaticalLayerCfg->iMaxSpatialBitrate	= priv->paramExt.iMaxBitrate;
	spaticalLayerCfg->uiProfileIdc			= PRO_BASELINE;
	
	// 单slice，有的解码器无法解码多slice
	spaticalLayerCfg->sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;


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
	if (cmResultSuccess != (*priv->encoder)->InitializeExt(priv->encoder, &priv->paramExt)) {
		log_error("InitializeExt fail.");
		return -1;
	}
	return 0;
}

static int openh264_enc_map_buf(struct encodec_object *obj, int buf_id) {
	return 0;
}

static int openh264_enc_unmap_buf(struct encodec_object *obj, int buf_id) {
	return 0;
}

static int openh264_frame_enc(struct encodec_object *obj, int buf_id) {
	int linesizes[4] = {0};
	struct raw_buffer *buffer;
	struct openh264_enc_data *priv = obj->priv;

	buffer = get_raw_buffer(obj->buf_pool, buf_id);

	linesizes[0] = buffer->width * 4;
	sws_scale(priv->sws_ctx, (const uint8_t *const *)buffer->ptrs, linesizes, 0, buffer->height, priv->frame_data, priv->linesize);

	// 初始化图片对象
	memset(&priv->pic, 0, sizeof(SSourcePicture));
	priv->pic.iPicWidth = buffer->width;
	priv->pic.iPicHeight = buffer->height;

	priv->pic.iColorFormat = videoFormatI420;
	priv->pic.iStride[0]	= priv->linesize[0];
	priv->pic.iStride[1]	= priv->linesize[1];
	priv->pic.iStride[2]	= priv->linesize[2];
	priv->pic.iStride[3]	= priv->linesize[3];
	priv->pic.pData[0]		= priv->frame_data[0];
	priv->pic.pData[1]		= priv->frame_data[1];
	priv->pic.pData[2]		= priv->frame_data[2];
	priv->pic.pData[3]		= priv->frame_data[3];

	int rv = (*priv->encoder)->EncodeFrame(priv->encoder, &priv->pic, &priv->info);
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

	priv->frame_count++;
	return 0;
}

static int openh264_force_i_frame(struct encodec_object *obj) {
	struct openh264_enc_data *priv = obj->priv;
	
	if ((*priv->encoder)->ForceIntraFrame(priv->encoder, true) != cmResultSuccess) {
		log_error("ForceIntraFrame fail.");
		return -1;
	}
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
	.name				= "openh264_encodec",
	.init				= openh264_enc_init,
	.set_info			= openh264_enc_set_info,
	.map_fb				= openh264_enc_map_buf,
	.unmap_fb			= openh264_enc_unmap_buf,
	.force_i			= openh264_force_i_frame,
	.put_fb				= openh264_frame_enc,
	.get_fb				= openh264_enc_getbuf,
	.release			= openh264_enc_release
};

ENCODEC_ADD_DEV(99, openh264_enc_ops)