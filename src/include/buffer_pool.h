#ifndef __BUFFER_POOL_H__
#define __BUFFER_POOL_H__
#define MAX_BUFFER_COUNT 128

int alloc_buffer();
int get_buffer();
int put_buffer(int buf_id);
int release_buffer(int buf_id);
int release_all_buffer();
struct raw_buffer* get_raw_buffer(int buf_id);
#endif