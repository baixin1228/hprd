#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "allocator.h"
#include "buffer_pool.h"
#include "frame_buffer.h"

int alloc_buffer(struct mem_pool *pool)
{
	for (int i = 0; i < MAX_BUFFER_COUNT; ++i)
	{
		if(pool->__buffers[i] == NULL)
		{
			pool->__buffers[i] = calloc(sizeof(struct raw_buffer), 1);
			pool->__buffers[i]->is_free = true;
			return i;
		}
	}
	return -1;
}

int alloc_buffer2(struct mem_pool *pool, uint32_t width, uint32_t height, uint32_t format)
{
	uint32_t bpp;
	struct drm_buffer * drm_buffer;
	for (int i = 0; i < MAX_BUFFER_COUNT; ++i)
	{
		if(pool->__buffers[i] == NULL)
		{
			pool->__buffers[i] = calloc(sizeof(struct raw_buffer), 1);
			bpp = format_to_bpp(format);
			drm_buffer = create_framebuffer(width, height, bpp);
			if(drm_buffer == NULL)
				return -1;
			pool->__buffers[i]->width = drm_buffer->width;
			pool->__buffers[i]->height = drm_buffer->height;
			pool->__buffers[i]->hor_stride = drm_buffer->width;
			pool->__buffers[i]->ver_stride = drm_buffer->height;
			pool->__buffers[i]->format = format;
			pool->__buffers[i]->bpp = bpp;
			pool->__buffers[i]->size = drm_buffer->size;
			pool->__buffers[i]->ptrs[0] = (char *)drm_buffer->vaddr;
			pool->__buffers[i]->priv = drm_buffer;
			pool->__buffers[i]->is_free = true;
			return i;
		}
	}
	return -1;
}

int __checkout_buf_id(struct raw_buffer** __buffers, int buf_id)
{
	if(buf_id >= MAX_BUFFER_COUNT)
		return -1;

	if(__buffers[buf_id] == NULL)
		return -1;

	return 0;
}

int get_fb(struct mem_pool *pool)
{
	for (int i = 0; i < MAX_BUFFER_COUNT; ++i)
	{
		if(pool->__buffers[i] != NULL && pool->__buffers[i]->is_free)
		{
			pool->__buffers[i]->is_free = false;
			return i;
		}
	}
	return -1;
}

int get_fb_block(struct mem_pool *pool, int timeout_ms)
{
	int buf_id;
	for(int j = 0; (buf_id = get_fb(pool)) == -1; j++)
	{
		if(j > timeout_ms)
		{
			log_error("get_fb time out", __func__);
			return -1;
		}
		usleep(1000);
	}
	return buf_id;
}

int put_fb(struct mem_pool *pool, int buf_id)
{
	if(__checkout_buf_id(pool->__buffers, buf_id) != 0)
		return -1;

	pool->__buffers[buf_id]->is_free = true;
	return 0;
}

int release_buffer(struct mem_pool *pool, int buf_id)
{
	if(__checkout_buf_id(pool->__buffers, buf_id) != 0)
		return -1;

	free(pool->__buffers[buf_id]);
	pool->__buffers[buf_id] = NULL;
	return 0;
}

int release_all_buffer(struct mem_pool *pool)
{
	for (int i = 0; i < MAX_BUFFER_COUNT; ++i)
	{
		if(pool->__buffers[i] != NULL)
		{
			free(pool->__buffers[i]);
			pool->__buffers[i] = NULL;
		}
	}
	return 0;
}

struct raw_buffer* get_raw_buffer(struct mem_pool *pool, int buf_id)
{
	if(__checkout_buf_id(pool->__buffers, buf_id) != 0)
		return NULL;

	return pool->__buffers[buf_id];
}