#ifndef __FRAME_BUFFER_H__
#define __FRAME_BUFFER_H__

#include "common.h"

enum FRAMEBUFFER_FORMAT
{
	RGB555,
	RGB565,
	RGB888,
	BGR888,
	ARGB8888,
	ABGR8888,
	RGBA8888,
	BGRA8888,
	YUV420P,
	NV12
};

struct raw_buffer
{
	uint32_t width;
	uint32_t hor_stride;
	uint32_t height;
	uint32_t ver_stride;
	uint32_t format;
	uint16_t bpp;
	uint32_t size;
	void *priv;
	char *ptrs[4];
	bool is_free;
};

int format_to_bpp(uint32_t format);
char* format_to_name(uint32_t format);
#endif