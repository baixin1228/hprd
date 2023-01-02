#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <tg_owt/third_party/openh264/src/codec/api/svc/codec_api.h>
#include <tg_owt/third_party/openh264/src/codec/api/svc/codec_ver.h>
#include "util.h"
#include "decodec.h"
#include "fb_convert.h"

#define OPENH264_VER_AT_LEAST(maj, min) \
    ((OPENH264_MAJOR  > (maj)) || \
     (OPENH264_MAJOR == (maj) && OPENH264_MINOR >= (min)))

struct svc_dec_data {
    ISVCDecoder *decoder;
    struct common_buffer frame_buffer;
    SBufferInfo info;
    uint8_t *ptrs[4];
    bool decode_success;
};


void _libopenh264_trace_callback(void *ctx, int level, const char *msg)
{
    log_info("[%p] %s", ctx, msg);
}

static int svc_decode_init(struct module_data *deccodec_dev, struct decodec_info enc_info)
{
    struct svc_dec_data *svc_data;
    SDecodingParam param = { 0 };
    int log_level;
    WelsTraceCallback callback_function;

    if(enc_info.stream_fmt != STREAM_H264)
        goto FAIL1;

    svc_data = calloc(1, sizeof(*svc_data));
    if(!svc_data)
    {
        func_error("calloc fail, check free memery.");
        goto FAIL1;
    }

    // OpenH264Version libver = WelsGetCodecVersion();

    if (WelsCreateDecoder(&svc_data->decoder)) {
        func_error("Unable to create decoder");
        goto FAIL2;
    }

    // Pass all libopenh264 messages to our callback, to allow ourselves to filter them.
    log_level = WELS_LOG_DETAIL;
    callback_function = _libopenh264_trace_callback;
    (*svc_data->decoder)->SetOption(svc_data->decoder, DECODER_OPTION_TRACE_LEVEL, &log_level);
    (*svc_data->decoder)->SetOption(svc_data->decoder, DECODER_OPTION_TRACE_CALLBACK, (void *)&callback_function);
    (*svc_data->decoder)->SetOption(svc_data->decoder, DECODER_OPTION_TRACE_CALLBACK_CONTEXT, (void *)&deccodec_dev);

#if !OPENH264_VER_AT_LEAST(1, 6)
    param.eOutputColorFormat = videoFormatI420;
#endif
    param.eEcActiveIdc       = ERROR_CON_DISABLE;
    param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

    if ((*svc_data->decoder)->Initialize(svc_data->decoder, &param) != cmResultSuccess) {
        func_error("openh264 Initialize failed\n");
        goto FAIL2;
    }

    svc_data->frame_buffer.format = YUV420P;
    svc_data->frame_buffer.bpp = 16;
    svc_data->frame_buffer.ptr = malloc(40 * 1204 * 1024);
    return 0;
FAIL2:
    free(svc_data);
FAIL1:
    return -1;
}

static int svc_push_pkt(struct module_data *encodec_dev, struct common_buffer *buffer)
{
    struct svc_dec_data *svc_data = encodec_dev->priv;
    DECODING_STATE state;
// #if OPENH264_VER_AT_LEAST(1, 7)
//     int opt;
// #endif
    svc_data->decode_success = false;

    if (!buffer->ptr) {
#if OPENH264_VER_AT_LEAST(1, 9)
        int end_of_stream = 1;
        (*svc_data->decoder)->SetOption(svc_data->decoder, DECODER_OPTION_END_OF_STREAM, &end_of_stream);
        state = (*svc_data->decoder)->FlushFrame(svc_data->decoder, svc_data->ptrs, &svc_data->info);
        svc_data->decode_success = true;
        return 0;
#else
        return 0;
#endif
    } else {
        // svc_data->info.uiInBsTimeStamp = buffer->pts;
#if OPENH264_VER_AT_LEAST(1, 4)
        // Contrary to the name, DecodeFrameNoDelay actually does buffering
        // and reordering of frames, and is the recommended decoding entry
        // point since 1.4. This is essential for successfully decoding
        // B-frames.
        state = (*svc_data->decoder)->DecodeFrameNoDelay(svc_data->decoder, (const unsigned char *)buffer->ptr, buffer->size, svc_data->ptrs, &svc_data->info);
#else
        state = (*svc_data->decoder)->DecodeFrame2(svc_data->decoder, (const unsigned char *)buffer->ptr, buffer->size, svc_data->ptrs, &svc_data->info);
#endif
    }
    if (state != dsErrorFree) {
        func_error("DecodeFrame failed");
        return -1;
    }
    if (svc_data->info.iBufferStatus != 1) {
        func_error("No frame produced");
        return -1;
    }

    svc_data->decode_success = true;
    return 0;
}

static struct common_buffer *svc_get_fb(struct module_data *encodec_dev)
{
    struct svc_dec_data *svc_data = encodec_dev->priv;

    if(!svc_data->decode_success)
    	return NULL;

    svc_data->frame_buffer.width = svc_data->info.UsrData.sSystemBuffer.iWidth;
    svc_data->frame_buffer.hor_stride = svc_data->info.UsrData.sSystemBuffer.iWidth;
    svc_data->frame_buffer.height = svc_data->info.UsrData.sSystemBuffer.iHeight;
    svc_data->frame_buffer.ver_stride = svc_data->info.UsrData.sSystemBuffer.iHeight;

    // linesize[0] = svc_data->info.UsrData.sSystemBuffer.iStride[0];
    // linesize[1] = linesize[2] = svc_data->info.UsrData.sSystemBuffer.iStride[1];
    memcpy(svc_data->frame_buffer.ptr, svc_data->ptrs[0], svc_data->info.UsrData.sSystemBuffer.iStride[0]);
    memcpy(svc_data->frame_buffer.ptr
    	+ svc_data->info.UsrData.sSystemBuffer.iStride[0],
    	svc_data->ptrs[1], svc_data->info.UsrData.sSystemBuffer.iStride[1]);
    memcpy(svc_data->frame_buffer.ptr
    	+ svc_data->info.UsrData.sSystemBuffer.iStride[0]
    	+ svc_data->info.UsrData.sSystemBuffer.iStride[1],
    	svc_data->ptrs[2], svc_data->info.UsrData.sSystemBuffer.iStride[1]);

//     avframe->pts     = svc_data->info.uiOutYuvTimeStamp;
//     avframe->pkt_dts = AV_NOPTS_VALUE;
// #if OPENH264_VER_AT_LEAST(1, 7)
//     (*s->decoder)->GetOption(s->decoder, DECODER_OPTION_PROFILE, &opt);
//     avctx->profile = opt;
//     (*s->decoder)->GetOption(s->decoder, DECODER_OPTION_LEVEL, &opt);
//     avctx->level = opt;
// #endif

    return &svc_data->frame_buffer;
}

static int svc_decode_close(struct module_data *deccodec_dev)
{
    struct svc_dec_data *svc_data = deccodec_dev->priv;

    if (svc_data->decoder)
        WelsDestroyDecoder(svc_data->decoder);

    if(svc_data->frame_buffer.ptr)
    	free(svc_data->frame_buffer.ptr);

    svc_data->frame_buffer.ptr = NULL;
    return 0;
}

struct decodec_ops libopenh264_decoder_dev = 
{
    .name               = "libopenh264",
    .init               = svc_decode_init,
    .push_pkt           = svc_push_pkt,
    .get_fb 	        = svc_get_fb,
    .release            = svc_decode_close
};

REGISTE_DECODEC_DEV(libopenh264_decoder_dev, DEVICE_PRIO_LOW);