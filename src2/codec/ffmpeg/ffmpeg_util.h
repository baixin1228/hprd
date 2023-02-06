#ifndef __FFMPEG_UTIL_H__
#define __FFMPEG_UTIL_H__
#include "frame_buffer.h"

int stream_fmt_to_av_fmt(int fmt);
int fb_fmt_to_av_fmt(enum FRAMEBUFFER_FORMAT format);
#endif