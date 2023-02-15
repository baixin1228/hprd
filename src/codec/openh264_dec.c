#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wels/codec_api.h>
#include <wels/codec_ver.h>
#include "util.h"
#include "codec.h"
#include "decodec.h"
#include "buffer_pool.h"

#define SVC_MAX_BUFFER_COUNT 24

#define OPENH264_VER_AT_LEAST(maj, min) \
    ((OPENH264_MAJOR  > (maj)) || \
     (OPENH264_MAJOR == (maj) && OPENH264_MINOR >= (min)))

struct svc_dec_data
{
    ISVCDecoder *decoder;
    SBufferInfo info;
    uint32_t format;
    uint32_t width;
    uint32_t height;
    struct svc_buf
    {
        bool is_free;
        int raw_buf_id;
    } svc_bufs[SVC_MAX_BUFFER_COUNT];
    uint16_t head_buf_id;
    uint16_t tail_buf_id;
};

void _libopenh264_trace_callback(void *ctx, int level, const char *msg)
{
    log_info("[%p] %s", ctx, msg);
}

static int svc_decode_init(struct decodec_object *obj)
{
    struct svc_dec_data *svc_data;
    SDecodingParam param = { 0 };
    int log_level;
    WelsTraceCallback callback_function;


    svc_data = calloc(1, sizeof(*svc_data));
    if(!svc_data)
    {
        log_error("calloc fail, check free memery.");
        goto FAIL1;
    }

    // OpenH264Version libver = WelsGetCodecVersion();

    if (WelsCreateDecoder(&svc_data->decoder))
    {
        log_error("Unable to create decoder");
        goto FAIL1;
    }

    // Pass all libopenh264 messages to our callback, to allow ourselves to filter them.
    log_level = WELS_LOG_DETAIL;
    callback_function = _libopenh264_trace_callback;
    (*svc_data->decoder)->SetOption(svc_data->decoder,
                                    DECODER_OPTION_TRACE_LEVEL, &log_level);
    (*svc_data->decoder)->SetOption(svc_data->decoder,
                                    DECODER_OPTION_TRACE_CALLBACK, (void *)&callback_function);
    (*svc_data->decoder)->SetOption(svc_data->decoder,
                                    DECODER_OPTION_TRACE_CALLBACK_CONTEXT, (void *)&obj);

#if !OPENH264_VER_AT_LEAST(1, 6)
    param.eOutputColorFormat = videoFormatI420;
#endif
    param.eEcActiveIdc       = ERROR_CON_DISABLE;
    param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

    if ((*svc_data->decoder)->Initialize(svc_data->decoder, &param) != cmResultSuccess)
    {
        log_error("openh264 Initialize failed\n");
        goto FAIL2;
    }
    svc_data->head_buf_id = 0;
    svc_data->tail_buf_id = 0;
    svc_data->format = YUV420P;

    return 0;
FAIL2:
    WelsDestroyDecoder(svc_data->decoder);
FAIL1:
    free(svc_data);
    return -1;
}

// static int svc_set_info(struct decodec_object *obj, GHashTable *dec_info)
// {
//     if(*(uint32_t *)g_hash_table_lookup(dec_info, "stream_fmt") != STREAM_H264)
//         return -1;

//     return 0;
// }

static int svc_map_buffer(struct decodec_object *obj, int buf_id)
{
    struct svc_buf *svc_buf;
    struct raw_buffer *raw_buf;
    struct svc_dec_data *svc_data = (struct svc_dec_data *)obj->priv;

    svc_buf = &svc_data->svc_bufs[buf_id];
    svc_buf->raw_buf_id = buf_id;
    svc_buf->is_free = true;

    raw_buf = get_raw_buffer(buf_id);

    raw_buf->format = YUV420P;
    raw_buf->bpp = 8;
    raw_buf->size = svc_data->width * svc_data->height * 3 / 2;

    return 0;
}

static int svc_push_pkt(struct decodec_object *obj, char *buf, size_t len)
{
    DECODING_STATE state;
    struct svc_buf *svc_buf;
    struct raw_buffer *raw_buf;
    struct svc_dec_data *svc_data = obj->priv;

    if(!svc_data->svc_bufs[svc_data->head_buf_id].is_free)
        return -1;

    svc_buf = &svc_data->svc_bufs[svc_data->head_buf_id];

    svc_data->head_buf_id++;
    svc_data->head_buf_id %= SVC_MAX_BUFFER_COUNT;

    raw_buf = get_raw_buffer(svc_buf->raw_buf_id);

    if (!buf)
    {
#if OPENH264_VER_AT_LEAST(1, 9)
        int end_of_stream = 1;
        (*svc_data->decoder)->SetOption(svc_data->decoder, DECODER_OPTION_END_OF_STREAM, &end_of_stream);
        state = (*svc_data->decoder)->FlushFrame(svc_data->decoder,
                (uint8_t **)raw_buf->ptrs, &svc_data->info);
        return 0;
#else
        return 0;
#endif
    }
    else
    {
        // svc_data->info.uiInBsTimeStamp = buffer->pts;
#if OPENH264_VER_AT_LEAST(1, 4)
        // Contrary to the name, DecodeFrameNoDelay actually does buffering
        // and reordering of frames, and is the recommended decoding entry
        // point since 1.4. This is essential for successfully decoding
        // B-frames.
        state = (*svc_data->decoder)->DecodeFrameNoDelay(svc_data->decoder,
                (const unsigned char *)buf, len, (uint8_t **)raw_buf->ptrs, &svc_data->info);
#else
        state = (*svc_data->decoder)->DecodeFrame2(svc_data->decoder,
                (const unsigned char *)buf, len, (uint8_t **)raw_buf->ptrs, &svc_data->info);
#endif
    }
    if (state != dsErrorFree)
    {
        log_error("DecodeFrame failed");
        return -1;
    }
    if (svc_data->info.iBufferStatus != 1)
    {
        log_error("No frame produced");
        return -1;
    }

    svc_data->width = svc_data->info.UsrData.sSystemBuffer.iWidth;
    svc_data->height = svc_data->info.UsrData.sSystemBuffer.iHeight;

    svc_buf->is_free = false;
    return 0;
}

static int svc_get_info(struct decodec_object *obj, GHashTable *fb_info)
{
    struct svc_dec_data *svc_data = obj->priv;

    if(svc_data->width == 0 || svc_data->height == 0)
        return -1;

    g_hash_table_insert(fb_info, "format", &svc_data->format);
    g_hash_table_insert(fb_info, "width", &svc_data->width);
    g_hash_table_insert(fb_info, "height", &svc_data->height);
    g_hash_table_insert(fb_info, "hor_stride", &svc_data->width);
    g_hash_table_insert(fb_info, "ver_stride", &svc_data->height);
    return 0;
}

static int svc_get_fb(struct decodec_object *obj)
{
    struct svc_buf *svc_buf;
    struct raw_buffer *raw_buf;
    struct svc_dec_data *svc_data = obj->priv;

    if(!svc_data->svc_bufs[svc_data->tail_buf_id].is_free)
        return -1;

    svc_buf = &svc_data->svc_bufs[svc_data->tail_buf_id];

    svc_data->tail_buf_id++;
    svc_data->tail_buf_id %= SVC_MAX_BUFFER_COUNT;

    raw_buf = get_raw_buffer(svc_buf->raw_buf_id);

    raw_buf->width = svc_data->info.UsrData.sSystemBuffer.iWidth;
    raw_buf->hor_stride = svc_data->info.UsrData.sSystemBuffer.iWidth;
    raw_buf->height = svc_data->info.UsrData.sSystemBuffer.iHeight;
    raw_buf->ver_stride = svc_data->info.UsrData.sSystemBuffer.iHeight;

    return svc_buf->raw_buf_id;
}

static int svc_put_fb(struct decodec_object *obj, int buf_id)
{
    struct svc_buf *svc_buf;
    struct svc_dec_data *svc_data = obj->priv;

    if(buf_id > SVC_MAX_BUFFER_COUNT || buf_id < 0)
        return -1;

    svc_buf = &svc_data->svc_bufs[buf_id];

    svc_buf->is_free = true;

    return 0;
}

static int svc_unmap_buffer(struct decodec_object *obj, int buf_id)
{
    return 0;
}

static int svc_decode_release(struct decodec_object *obj)
{
    struct svc_dec_data *svc_data = obj->priv;

    if (svc_data->decoder)
        WelsDestroyDecoder(svc_data->decoder);

    return 0;
}

struct decodec_ops dev_ops =
{
    .name               = "libopenh264",
    .init               = svc_decode_init,
    .put_pkt            = svc_push_pkt,
    .get_info           = svc_get_info,
    .map_buffer         = svc_map_buffer,
    .get_buffer         = svc_get_fb,
    .put_buffer         = svc_put_fb,
    .unmap_buffer       = svc_unmap_buffer,
    .release            = svc_decode_release
};
