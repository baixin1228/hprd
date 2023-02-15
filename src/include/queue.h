#ifndef __MINI_RING_QUEUE_H__
#define __MINI_RING_QUEUE_H__
#include <stdint.h>

#define QUEUE_SIZE 24

struct int_queue
{
	int buffer_queue[QUEUE_SIZE];
	uint32_t buffer_head;
	uint32_t buffer_tail;
};

#define QUEUE_DATA_SIZE (8*1024*1024)
struct data_queue
{
	char buffer_queue[QUEUE_DATA_SIZE];
	uint64_t buffer_head;
	uint64_t buffer_tail;
};

enum MINI_QUEUE_RET
{
	QUEUE_OK,
	QUEUE_ERR,
	QUEUE_FULL,
	QUEUE_NO_DATA,
	OVER_SIZE,
};

int queue_put_int(struct int_queue *queue, int id);
int queue_get_int(struct int_queue *queue);

int write_queue_data(struct data_queue *queue, uint8_t *buf, size_t len);
int read_queue_data(struct data_queue *queue, uint8_t *buf, size_t len);
int get_queue_data_len(struct data_queue *queue);

#endif  /* __MINI_RING_QUEUE_H__ */