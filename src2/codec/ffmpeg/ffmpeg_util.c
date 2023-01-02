#include <libavformat/avformat.h>

#include "codec.h"

int com_fmt_to_av_fmt(int fmt)
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
            return -1;
        break;
    }
}