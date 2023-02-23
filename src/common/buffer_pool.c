#include <malloc.h>
#include <string.h>

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

int __checkout_buf_id(struct raw_buffer** __buffers, int buf_id)
{
	if(buf_id >= MAX_BUFFER_COUNT)
		return -1;

	if(__buffers[buf_id] == NULL)
		return -1;

	return 0;
}

int get_buffer(struct mem_pool *pool)
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

int put_buffer(struct mem_pool *pool, int buf_id)
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