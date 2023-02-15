#ifndef __BUFFER_POOL_H__
#define __BUFFER_POOL_H__
#define MAX_BUFFER_COUNT 128

int alloc_buffer();
int free_buffer(int buf_id);
struct raw_buffer* get_raw_buffer(int buf_id);
#endif