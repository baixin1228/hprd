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

int queue_put_int(struct int_queue *queue, int id);
int queue_get_int(struct int_queue *queue);

int queue_append_data(struct data_queue *queue, void *p_buf, size_t len);
int pop_queue_data(struct data_queue *queue, void *p_buf, size_t len);
int queue_get_free_count(struct data_queue *queue);
int get_queue_data_count(struct data_queue *queue);
int copy_queue_data(struct data_queue *queue, uint8_t *buf, size_t len);
int queue_tail_point_forward(struct data_queue *queue, size_t len);
int write2_queue_data(struct data_queue *queue, uint8_t *buf, size_t len);
int queue_head_point_forward(struct data_queue *queue, size_t len);
char *queue_get_write_ptr(struct data_queue *queue);
char *queue_get_read_ptr(struct data_queue *queue);
int queue_get_end_data_count(struct data_queue *queue);
int queue_get_end_free_count(struct data_queue *queue);

#endif  /* __MINI_RING_QUEUE_H__ */