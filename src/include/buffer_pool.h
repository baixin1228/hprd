#ifndef __BUFFER_POOL_H__
#define __BUFFER_POOL_H__
#define MAX_BUFFER_COUNT 64

struct mem_pool
{
	struct raw_buffer* __buffers[MAX_BUFFER_COUNT];
};

int alloc_buffer(struct mem_pool *pool);
int alloc_buffer2(struct mem_pool *pool, uint32_t width, uint32_t height, uint32_t format);
int get_fb(struct mem_pool *pool);
int get_fb_block(struct mem_pool *pool, int timeout_ms);
int put_fb(struct mem_pool *pool, int buf_id);
int release_buffer(struct mem_pool *pool, int buf_id);
int release_all_buffer(struct mem_pool *pool);
struct raw_buffer* get_raw_buffer(struct mem_pool *pool, int buf_id);
#endif