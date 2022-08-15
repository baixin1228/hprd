#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "list.h"

enum COMMON_BUFFER_FORMAT
{
	RGB444,
	RGB888,
	YUV420P,
	NV12,
};

struct common_buffer
{
	/* is it drm_buffer */
	bool is_dma;
	/* is it bind to egl */
	bool is_bind;

	enum COMMON_BUFFER_FORMAT format;
	uint32_t width;
	uint32_t height;
	uint32_t hor_stride;
	uint32_t ver_stride;
	uint64_t size;
	uint32_t pitch;
	uint8_t bpp;

	/* buffer vaddr */
	char* ptr;
	/* dma buffer fd */
	int32_t fd;
	/* dma buffer hnd */
	uint32_t handle;

	/* is it in the free pool */
	bool is_free;
	struct list_node node;
};


struct frame_buffer_info
{
	enum COMMON_BUFFER_FORMAT format;
	uint32_t width;
	uint32_t height;
	uint32_t hor_stride;
	uint32_t ver_stride;
};
#endif