#ifndef BUFFER_H
#define BUFFER_H

#include "list.h"

struct common_buffer
{
	/* is it drm_buffer */
	bool is_dma;
	/* is it bind to egl */
	bool is_bind;

	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t h_stride;
	uint32_t v_stride;
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

#endif