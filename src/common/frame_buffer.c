#include "frame_buffer.h"

inline int format_to_bpp(uint32_t format)
{
	switch(format)
	{
		case RGB555:
		case RGB565:
			return 16;
		case RGB888:
		case BGR888:
			return 24;
		case ARGB8888:
		case ABGR8888:
		case RGBA8888:
		case BGRA8888:
			return 32;
		case YUV420P:
		case NV12:
			return 12;
	}

	return -1;
}

inline char* format_to_name(uint32_t format)
{
	switch(format)
	{
		case RGB555:
			return "RGB555";
		case RGB565:
			return "RGB565";
		case RGB888:
			return "RGB888";
		case BGR888:
			return "BGR888";
		case ARGB8888:
			return "ARGB8888";
		case ABGR8888:
			return "ABGR8888";
		case RGBA8888:
			return "RGBA8888";
		case BGRA8888:
			return "BGRA8888";
		case YUV420P:
			return "YUV420P";
		case NV12:
			return "NV12";
	}

	return "unknow frame";
}