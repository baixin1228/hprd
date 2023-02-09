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
			return i;
		}
	}
	return -1;
}

int free_buffer(int buf_id)
{
	if(buf_id >= MAX_BUFFER_COUNT)
		return -1;

	if(__buffers[buf_id] == NULL)
		return -1;

	free(__buffers[buf_id]);
	__buffers[buf_id] = NULL;
	return 0;
}

struct raw_buffer* get_raw_buffer(int buf_id)
{
	if(buf_id >= MAX_BUFFER_COUNT)
		return NULL;

	if(__buffers[buf_id] == NULL)
		return NULL;

	return __buffers[buf_id];
}