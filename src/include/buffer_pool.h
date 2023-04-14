#ifndef __BUFFER_POOL_H__
#define __BUFFER_POOL_H__
#define MAX_BUFFER_COUNT 64

struct mem_pool
{
	struct raw_buffer* __buffers[MAX_BUFFER_COUNT];
};

int alloc_buffer(struct mem_pool *pool);
int get_fb(struct mem_pool *pool);
int put_fb(struct mem_pool *pool, int buf_id);
int release_buffer(struct mem_pool *pool, int buf_id);
int release_all_buffer(struct mem_pool *pool);
struct raw_buffer* get_raw_buffer(struct mem_pool *pool, int buf_id);
#endif