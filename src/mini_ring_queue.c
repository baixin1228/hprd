#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include "util.h"
#include "mini_ring_queue.h"

void put_buffer_id(struct mini_id_queue *queue, int id)
{
	if(queue == NULL)
		return;
	if(atomic_load(&queue->buffer_head)<= atomic_load(&queue->buffer_tail) + QUEUE_SIZE)
	{
		queue->buffer_queue[queue->buffer_head++ % QUEUE_SIZE] = id;
		return;
	}

	func_error("buffer full.");
}

int get_buffer_id(struct mini_id_queue *queue)
{
	if(queue == NULL)
		return -1;
	if(atomic_load(&queue->buffer_tail) < atomic_load(&queue->buffer_head))
	{
		return queue->buffer_queue[queue->buffer_tail++ % QUEUE_SIZE];
	}
	return -1;
}

int write_queue_data(struct mini_data_queue *queue, uint8_t *buf, size_t len)
{
	if(queue == NULL)
	{
    	func_error("queue is null.");
		return QUEUE_ERR;
	}
	if(len > QUEUE_DATA_SIZE)
	{
    	func_error("write size greater than buffer.");
    	func_error("写入数据过大，请减小一次写入量.");
		return OVER_SIZE;
	}

	if(len <= atomic_load(&queue->buffer_tail) + QUEUE_DATA_SIZE - atomic_load(&queue->buffer_head))
	{
		if(len <= QUEUE_DATA_SIZE - (queue->buffer_head % QUEUE_DATA_SIZE))
		{
			memcpy(&queue->buffer_queue[queue->buffer_head % QUEUE_DATA_SIZE], buf, len);
			queue->buffer_head += len;
		}else{
			size_t len_tmp = QUEUE_DATA_SIZE - (queue->buffer_head % QUEUE_DATA_SIZE);
			memcpy(&queue->buffer_queue[queue->buffer_head % QUEUE_DATA_SIZE], buf, len_tmp);
			queue->buffer_head += len_tmp;
			buf += len_tmp;
			memcpy(&queue->buffer_queue[queue->buffer_head % QUEUE_DATA_SIZE], buf, len - len_tmp);
			queue->buffer_head += (len - len_tmp);
		}

		return len;
	}
	return QUEUE_FULL;
}

int read_queue_data(struct mini_data_queue *queue, uint8_t *buf, size_t len)
{
	if(queue == NULL)
	{
    	func_error("queue is null.");
		return QUEUE_ERR;
	}
	if(len > QUEUE_DATA_SIZE)
	{
    	func_error("read size greater than buffer.");
		return OVER_SIZE;
	}

	if(len <= atomic_load(&queue->buffer_head) - atomic_load(&queue->buffer_tail))
	{
		if(len <= QUEUE_DATA_SIZE - (queue->buffer_tail % QUEUE_DATA_SIZE))
		{
			memcpy(buf, &queue->buffer_queue[queue->buffer_tail % QUEUE_DATA_SIZE], len);
			queue->buffer_tail += len;
		}else{
			size_t len_tmp = QUEUE_DATA_SIZE - (queue->buffer_tail % QUEUE_DATA_SIZE);
			memcpy(buf, &queue->buffer_queue[queue->buffer_tail % QUEUE_DATA_SIZE], len_tmp);
			queue->buffer_tail += len_tmp;
			buf += len_tmp;
			memcpy(buf, &queue->buffer_queue[queue->buffer_tail % QUEUE_DATA_SIZE], len - len_tmp);
			queue->buffer_tail += (len - len_tmp);
		}

		return len;
	}
	return QUEUE_NO_DATA;
}

int get_queue_data_len(struct mini_data_queue *queue)
{
	if(queue == NULL)
		return QUEUE_ERR;
	return atomic_load(&queue->buffer_head) - atomic_load(&queue->buffer_tail);
}
