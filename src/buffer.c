#include "buffer.h"

uint32_t get_line_size(struct common_buffer *buf)
{
	uint32_t ret = 0;
	switch(buf->format)
	{
		case RGB444:
			ret = buf->hor_stride * buf->bpp;
		break;
		case RGB888:
			ret = buf->hor_stride * buf->bpp;
		break;
		case YUV420P:
			ret = buf->hor_stride;
		break;
		case NV12:
			ret = buf->hor_stride;
		break;
		default:
			ret = buf->hor_stride * buf->bpp;
		break;
	}
	return ret;
}