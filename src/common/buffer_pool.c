#include <malloc.h>
#include <string.h>

#include "buffer_pool.h"
#include "frame_buffer.h"

static struct raw_buffer* __buffers[MAX_BUFFER_COUNT] = {0};

int alloc_buffer()
{
	for (int i = 0; i < MAX_BUFFER_COUNT; ++i)
	{
		if(__buffers[i] == NULL)
		{
			__buffers[i] = calloc(sizeof(struct raw_buffer), 1);
			__buffers[i]->is_free = true;
			return i;
		}
	}
	return -1;
}

int __checkout_buf_id(int buf_id)
{
	if(buf_id >= MAX_BUFFER_COUNT)
		return -1;

	if(__buffers[buf_id] == NULL)
		return -1;

	return 0;
}

int get_buffer()
{
	for (int i = 0; i < MAX_BUFFER_COUNT; ++i)
	{
		if(__buffers[i] != NULL && __buffers[i]->is_free)
		{
			__buffers[i]->is_free = false;
			return i;
		}
	}
	return -1;
}

int put_buffer(int buf_id)
{
	if(__checkout_buf_id(buf_id) != 0)
		return -1;

	__buffers[buf_id]->is_free = true;
	return 0;
}

int release_buffer(int buf_id)
{
	if(__checkout_buf_id(buf_id) != 0)
		return -1;

	free(__buffers[buf_id]);
	__buffers[buf_id] = NULL;
	return 0;
}

int release_all_buffer()
{
	for (int i = 0; i < MAX_BUFFER_COUNT; ++i)
	{
		if(__buffers[i] != NULL)
		{
			free(__buffers[i]);
			__buffers[i] = NULL;
		}
	}
	return 0;
}

struct raw_buffer* get_raw_buffer(int buf_id)
{
	if(__checkout_buf_id(buf_id) != 0)
		return NULL;

	return __buffers[buf_id];
}