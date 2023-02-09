#include <libavformat/avformat.h>

#include "codec.h"
#include "ffmpeg_util.h"

int stream_fmt_to_av_fmt(int fmt) {
    switch(fmt) {
    case STREAM_H264:
        return AV_CODEC_ID_H264;
        break;
    case STREAM_H265:
        return AV_CODEC_ID_H265;
        break;
    default:
        return -1;
        break;
    }
}

int fb_fmt_to_av_fmt(enum FRAMEBUFFER_FORMAT format) {
    switch(format) {
    case ARGB8888:
        return AV_PIX_FMT_RGB32;
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