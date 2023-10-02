#ifndef __ALLOCATOR_H__
#define __ALLOCATOR_H__
#include <stdint.h>
#include <stdbool.h>

struct drm_buffer {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint8_t *vaddr;
};

struct drm_buffer *create_framebuffer(uint32_t width, uint32_t height, uint16_t bpp);
void release_framebuffer(struct drm_buffer *fb);
#endif