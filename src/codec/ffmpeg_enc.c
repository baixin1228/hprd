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

struct ffmpeg_enc_data {
    int capture_fb_fmt;

    AVFormatContext *av_ctx;
    AVCodecContext  *av_codec_ctx;
    AVCodec         *av_codec;

    /* capture and display */
    AVPacket        *av_packet;
    AVFrame         *av_frame;

    struct SwsContext *sws_ctx;

    int enc_status;
    struct int_queue id_queue;
};

static int ffmpeg_enc_init(struct encodec_object *obj) {
    struct ffmpeg_enc_data *enc_data;

    enc_data = calloc(1, sizeof(*enc_data));
    if(!enc_data) {
        log_error("calloc fail, check free memery.");
        return -1;
    }

    obj->priv = enc_data;
    return 0;
}

static int ffmpeg_enc_set_info(
    struct encodec_object *obj, GHashTable *enc_info) {
    int ret;
    uint32_t frame_rate;
    int stream_format;
    AVCodecContext *av_codec_ctx = NULL;
    struct ffmpeg_enc_data *enc_data = obj->priv;

    if(g_hash_table_contains(enc_info, "frame_rate"))
        frame_rate = *(uint32_t *)g_hash_table_lookup(enc_info, "frame_rate");
    else
        frame_rate = 30;
    
    stream_format = stream_fmt_to_av_fmt(
                        *(uint32_t *)g_hash_table_lookup(enc_info, "stream_fmt"));
    enc_data->av_codec = avcodec_find_encoder(stream_format);
    if (!enc_data->av_codec) {
        log_error("ffmpeg enc avcodec not found.");
        goto FAIL1;
    }

    enc_data->av_codec_ctx = avcodec_alloc_context3(enc_data->av_codec);
    if (!enc_data->av_codec_ctx) {
        log_error("ffmpeg Could not allocate video codec context");
        goto FAIL1;
    }
    av_codec_ctx = enc_data->av_codec_ctx;

    av_codec_ctx->codec_id = stream_format;
    av_codec_ctx->bit_rate = *(uint32_t *)g_hash_table_lookup(
                                 enc_info, "bit_rate");
    av_codec_ctx->width = *(uint32_t *)g_hash_table_lookup(enc_info, "width");
    av_codec_ctx->height = *(uint32_t *)g_hash_table_lookup(enc_info, "height");
    av_codec_ctx->time_base = (AVRational) {
        1, frame_rate
    };
    av_codec_ctx->framerate = (AVRational) {
        frame_rate, 1
    };
    enc_data->capture_fb_fmt = fb_fmt_to_av_fmt(
                                 *(uint32_t *)g_hash_table_lookup(enc_info, "format"));

    if(enc_data->capture_fb_fmt == AV_PIX_FMT_RGB32) {
        av_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

        enc_data->sws_ctx = sws_getContext(
                                av_codec_ctx->width,
                                av_codec_ctx->height,
                                enc_data->capture_fb_fmt,
                                av_codec_ctx->width,
                                av_codec_ctx->height,
                                av_codec_ctx->pix_fmt,
                                SWS_POINT, NULL, NULL, NULL);
    } else {
        av_codec_ctx->pix_fmt = enc_data->capture_fb_fmt;
    }

    av_codec_ctx->gop_size = frame_rate;
    av_codec_ctx->max_b_frames = 0;
    av_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    av_codec_ctx->thread_count = 2;

    AVDictionary *param = 0;

    if(av_codec_ctx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    if(av_codec_ctx->codec_id == AV_CODEC_ID_H265) {
        av_dict_set(&param, "preset", "ultrafast", 0);
        // av_dict_set(&param, "x265-params", "qp=20", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    if (avcodec_open2(av_codec_ctx, enc_data->av_codec, &param) < 0) {
        log_error("ffmpeg Could not open codec");
        goto FAIL2;
    }

    enc_data->av_frame = av_frame_alloc();
    if (!enc_data->av_frame) {
        log_error("Could not allocate video frame");
        goto FAIL2;
    }
    enc_data->av_frame->format = av_codec_ctx->pix_fmt;
    enc_data->av_frame->width  = av_codec_ctx->width;
    enc_data->av_frame->height = av_codec_ctx->height;

    if(av_codec_ctx->pix_fmt != enc_data->capture_fb_fmt) {
        ret = av_frame_get_buffer(enc_data->av_frame, 0);
        if (ret < 0) {
            log_error("Could not allocate the video frame data");
            goto FAIL3;
        }
    }

    enc_data->av_packet = av_packet_alloc();
    if (!enc_data->av_packet) {
        log_error("ffmpeg enc alloc pkg fail.");
        goto FAIL3;
    }

    return 0;
FAIL3:
    av_frame_free(&enc_data->av_frame);
FAIL2:
    avcodec_close(enc_data->av_codec_ctx);
FAIL1:
    return -1;
}

static void _ffmpeg_enc_get_pkt(struct encodec_object *obj) {
    struct ffmpeg_enc_data *enc_data = obj->priv;
    AVPacket *pkt = enc_data->av_packet;
    AVCodecContext *av_codec_ctx = enc_data->av_codec_ctx;

    if (enc_data->enc_status >= 0) {
        enc_data->enc_status = avcodec_receive_packet(av_codec_ctx, pkt);
        if (enc_data->enc_status == AVERROR_EOF) {
            log_info("encode end.");
            return;
        }
        if (enc_data->enc_status == AVERROR(EAGAIN)) {
            log_info("not data, retry.");
            return;
        } else if (enc_data->enc_status < 0) {
            log_error("Error during encoding");
            return;
        }

        if (pkt->flags & AV_PKT_FLAG_KEY) { //找到带I帧的AVPacket
            // log_info("-------> ffmpeg enc:find I frame size:%d.", pkt->size);
            //找到I帧，插入SPS和PPS
            obj->pkt_callback((char *)av_codec_ctx->extradata, av_codec_ctx->extradata_size);
        } else {
            // log_info("-------> ffmpeg enc:find P frame size:%d.", pkt->size);
        }

        obj->pkt_callback((char *)pkt->data, pkt->size);

        av_packet_unref(pkt);
    } else {
        log_error("get pkt error.");
    }
}

static int ffmpeg_enc_map_buf(struct encodec_object *obj, int buf_id) {
    return 0;
}

static int ffmpeg_enc_unmap_buf(struct encodec_object *obj, int buf_id) {
    return 0;
}

static int ffmpeg_frame_enc(struct encodec_object *obj, int buf_id) {
    struct raw_buffer *buffer;
    struct ffmpeg_enc_data *enc_data = obj->priv;

    AVFrame *frame = enc_data->av_frame;
    buffer = get_raw_buffer(obj->buf_pool, buf_id);

    int linesizes[4] = {
        buffer->size / buffer->height, 0, 0, 0
    };
    if(enc_data->av_codec_ctx->pix_fmt != enc_data->capture_fb_fmt) {
        /* convert */
        sws_scale(enc_data->sws_ctx, (const uint8_t *const *)buffer->ptrs, linesizes, 0, buffer->height, frame->data, frame->linesize);
    } else {
        /* zero copy */
        switch(frame->format) {
        case AV_PIX_FMT_YUV420P:
            frame->data[0] = (uint8_t *)buffer->ptrs[0];
            frame->data[1] = (uint8_t *)buffer->ptrs[0] + buffer->width * buffer->height;
            frame->data[2] = (uint8_t *)buffer->ptrs[0] + buffer->width * buffer->height +
                             buffer->width * buffer->height / 4;
            break;
        case AV_PIX_FMT_NV12:
            frame->data[0] = (uint8_t *)buffer->ptrs[0];
            frame->data[1] = (uint8_t *)buffer->ptrs[0] + buffer->width * buffer->height;
            frame->data[2] = (uint8_t *)0;
            break;
        default:
            log_error("unsupport frame format.");
            break;
        }
    }

    enc_data->enc_status = avcodec_send_frame(enc_data->av_codec_ctx,
                           frame);
    if (enc_data->enc_status < 0) {
        log_error("Error sending a frame for encoding");
        return -1;
    }

    _ffmpeg_enc_get_pkt(obj);

    while(queue_put_int(&enc_data->id_queue, buf_id) != 0)
        log_warning("queue_put_int fail.");

    return 0;
}

static int ffmpeg_enc_getbuf(struct encodec_object *obj) {
    int ret;
    struct ffmpeg_enc_data *enc_data = obj->priv;

    while((ret = queue_get_int(&enc_data->id_queue)) == -1)
        log_warning("queue_get_int fail.");

    return ret;
}

static int ffmpeg_enc_release(struct encodec_object *obj) {
    struct ffmpeg_enc_data *enc_data = obj->priv;
    avcodec_close(enc_data->av_codec_ctx);
    av_packet_free(&enc_data->av_packet);
    if(enc_data->sws_ctx) {
        sws_freeContext(enc_data->sws_ctx);
        enc_data->sws_ctx = NULL;
    }
    return 0;
}

struct encodec_ops ffmpeg_enc_ops = {
    .name               = "ffmpeg_encodec",
    .init               = ffmpeg_enc_init,
    .set_info           = ffmpeg_enc_set_info,
    .map_buffer         = ffmpeg_enc_map_buf,
    .unmap_buffer       = ffmpeg_enc_unmap_buf,
    .put_buffer         = ffmpeg_frame_enc,
    .get_buffer         = ffmpeg_enc_getbuf,
    .release            = ffmpeg_enc_release
};