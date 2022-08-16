#include <stdio.h>
#include <stdatomic.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "util.h"
#include "codec/encodec.h"

struct ffmpeg_enc_data{
    AVFormatContext *av_ctx;
    AVCodecContext  *av_codec_ctx;
    AVCodec         *av_codec;

    /* input and output */
    AVPacket        *av_packet;
    AVFrame         *av_frame;

    struct common_buffer ret_pkt;
    int enc_status;
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

static int _com_fmt_to_ff_fmt(int fmt)
{
    switch(fmt)
    {
        case STREAM_H264:
            return AV_CODEC_ID_H264;
        break;
        case STREAM_H265:
            return AV_CODEC_ID_H265;
        break;
        default:
            return AV_CODEC_ID_H265;
        break;
    }
}

static int ffmpeg_enc_init(struct module_data *encodec_dev, struct encodec_info enc_info)
{
    int ret;
    int format;
    AVCodecContext *av_codec_ctx= NULL;
    struct ffmpeg_enc_data *enc_data;

    enc_data = calloc(1, sizeof(*enc_data));
    if(!enc_data)
    {
        func_error("calloc fail, check free memery.");
        goto FAIL1;
    }

    format = _com_fmt_to_ff_fmt(enc_info.stream_fmt);
    enc_data->av_codec = avcodec_find_encoder(format);
    if (!enc_data->av_codec) {
        func_error("ffmpeg enc avcodec not found.");
        goto FAIL2;
    }

    enc_data->av_codec_ctx = avcodec_alloc_context3(enc_data->av_codec);
    if (!enc_data->av_codec_ctx) {
        func_error("ffmpeg Could not allocate video codec context");
        goto FAIL2;
    }
    av_codec_ctx = enc_data->av_codec_ctx;

    av_codec_ctx->codec_id = format;
    av_codec_ctx->bit_rate = 30 * 1204 * 1204;
    av_codec_ctx->width = enc_info.fb_info.width;
    av_codec_ctx->height = enc_info.fb_info.height;
    av_codec_ctx->time_base = (AVRational){1, 25};
    av_codec_ctx->framerate = (AVRational){25, 1};
    av_codec_ctx->pix_fmt = _com_fb_fmt_to_av_fmt(enc_info.fb_info.format);
    av_codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    av_codec_ctx->gop_size = 30;
    av_codec_ctx->max_b_frames = 0;
    av_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    av_codec_ctx->thread_count = 4;

    AVDictionary *param = 0;

    if(av_codec_ctx->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }

    if(av_codec_ctx->codec_id == AV_CODEC_ID_HEVC || av_codec_ctx->codec_id == AV_CODEC_ID_H265){
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "x265-params", "qp=20", 0);
        av_dict_set(&param, "tune", "zero-latency", 0);
    }

    if (avcodec_open2(av_codec_ctx, enc_data->av_codec, &param) < 0) {
        func_error("ffmpeg Could not open codec");
        goto FAIL3;
    }
    
    enc_data->av_frame = av_frame_alloc();
    if (!enc_data->av_frame) {
        func_error("Could not allocate video frame");
        goto FAIL3;
    }
    enc_data->av_frame->format = av_codec_ctx->pix_fmt;
    enc_data->av_frame->width  = av_codec_ctx->width;
    enc_data->av_frame->height = av_codec_ctx->height;
 
    ret = av_frame_get_buffer(enc_data->av_frame, 0);
    if (ret < 0) {
        log_error("Could not allocate the video frame data");
        goto FAIL4;
    }

    enc_data->av_packet = av_packet_alloc();
    if (!enc_data->av_packet)
    {
        func_error("ffmpeg enc alloc pkg fail.");
        goto FAIL4;
    }

    encodec_dev->priv = enc_data;

    return 0;
FAIL4:
    av_frame_free(&enc_data->av_frame);
FAIL3:
    avcodec_close(enc_data->av_codec_ctx);
FAIL2:
    free(enc_data);
FAIL1:
    return -1;
}

static int ffmpeg_frame_enc(struct module_data *encodec_dev, struct common_buffer *buffer)
{
    struct ffmpeg_enc_data *enc_data = encodec_dev->priv;
    AVFrame *frame = enc_data->av_frame;

    switch(frame->format)
    {
        case AV_PIX_FMT_RGB444:
        case AV_PIX_FMT_RGB24:
            frame->data[0] = (uint8_t *)buffer->ptr;
            frame->data[1] = (uint8_t *)0;
            frame->data[2] = (uint8_t *)0;
        break;
        case AV_PIX_FMT_YUV420P:
            frame->data[0] = (uint8_t *)buffer->ptr;
            frame->data[1] = (uint8_t *)buffer->ptr + buffer->width * buffer->height;
            frame->data[2] = (uint8_t *)buffer->ptr + buffer->width * buffer->height +
                                buffer->width * buffer->height / 4;
        break;
        case AV_PIX_FMT_NV12:
            frame->data[0] = (uint8_t *)buffer->ptr;
            frame->data[1] = (uint8_t *)buffer->ptr + buffer->width * buffer->height;
            frame->data[2] = (uint8_t *)0;
        break;
        default:
            func_error("unsupport frame format.");
        break;
    }

    enc_data->enc_status = avcodec_send_frame(enc_data->av_codec_ctx, enc_data->av_frame);
    if (enc_data->enc_status < 0) {
        func_error("Error sending a frame for encoding");
        return -1;
    }
    return 0;
}

static  struct common_buffer *ffmpeg_enc_get_pkt(struct module_data *encodec_dev)
{
    struct ffmpeg_enc_data *enc_data = encodec_dev->priv;
    AVPacket *pkt = enc_data->av_packet;
    AVCodecContext *av_codec_ctx = enc_data->av_codec_ctx;

    if (enc_data->enc_status >= 0) {
        enc_data->enc_status = avcodec_receive_packet(av_codec_ctx, pkt);
        if (enc_data->enc_status == AVERROR(EAGAIN) || enc_data->enc_status == AVERROR_EOF)
        {
            log_info("encode end.");
            return NULL;
        }
        else if (enc_data->enc_status < 0) {
            func_error("Error during encoding");
            return NULL;
        }

        if (pkt->flags & AV_PKT_FLAG_KEY)//找到带I帧的AVPacket
        {
            log_info("ffmpeg enc:find I frame.");
            //找到I帧，插入SPS和PPS
            // code_ctx->on_package(av_codec_ctx->extradata, av_codec_ctx->extradata_size);
        }else{
            log_info("ffmpeg enc:find P frame.");
            // code_ctx->on_package(pkt->data, pkt->size);
        }
        av_packet_unref(pkt);
    }
    func_error("get pkt error.");
    return NULL;
}

static int ffmpeg_enc_release(struct module_data *encodec_dev)
{
    struct ffmpeg_enc_data *enc_data = encodec_dev->priv;
    avcodec_close(enc_data->av_codec_ctx);
    av_packet_free(&enc_data->av_packet);
    return 0;
}

struct encodec_ops ffmpeg_encodec_dev = 
{
    .name               = "ffmpeg_encodec_dev",
    .init               = ffmpeg_enc_init,
    .push_fb            = ffmpeg_frame_enc,
    .get_package        = ffmpeg_enc_get_pkt,
    .release            = ffmpeg_enc_release
};

REGISTE_ENCODEC_DEV(ffmpeg_encodec_dev, DEVICE_PRIO_LOW);