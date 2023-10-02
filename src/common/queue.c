#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include "util.h"
#include "queue.h"

int queue_put_int(struct int_queue *queue, int id)
{
	if(queue == NULL)
		return -1;
	if(atomic_load(&queue->buffer_head)<= atomic_load(&queue->buffer_tail) + QUEUE_SIZE)
	{
		queue->buffer_queue[queue->buffer_head++ % QUEUE_SIZE] = id;
		return 0;
	}

	log_warning("buffer full.");
	return -1;
}

int queue_get_int(struct int_queue *queue)
{
	if(queue == NULL)
		return -1;
	if(atomic_load(&queue->buffer_tail) < atomic_load(&queue->buffer_head))
	{
		return queue->buffer_queue[queue->buffer_tail++ % QUEUE_SIZE];
	}
	return -1;
}

int queue_int_len(struct int_queue *queue)
{
	if(queue == NULL)
		return -1;
	return queue->buffer_head - queue->buffer_tail;
}

int enqueue_data(struct data_queue *queue, void *p_buf, size_t len)
{
	char *buf = (char *)p_buf;
	if(queue == NULL)
	{
		return -1;
	}
	if(len > QUEUE_DATA_SIZE)
	{
    	log_error("write size greater than buffer.");
		return -1;
	}

	if(len <= atomic_load(&queue->buffer_tail) + QUEUE_DATA_SIZE - atomic_load(&queue->buffer_head))
	{
		if(len <= QUEUE_DATA_SIZE - (queue->buffer_head % QUEUE_DATA_SIZE))
		{
			memcpy(&queue->buffer_queue[queue->buffer_head % QUEUE_DATA_SIZE], buf, len);
			atomic_fetch_add(&queue->buffer_head, len);
		}else{
			size_t len_tmp = QUEUE_DATA_SIZE - (queue->buffer_head % QUEUE_DATA_SIZE);
			memcpy(&queue->buffer_queue[queue->buffer_head % QUEUE_DATA_SIZE], buf, len_tmp);
			atomic_fetch_add(&queue->buffer_head, len_tmp);
			buf += len_tmp;
			memcpy(&queue->buffer_queue[queue->buffer_head % QUEUE_DATA_SIZE], buf, len - len_tmp);
			atomic_fetch_add(&queue->buffer_head, (len - len_tmp));
		}

		return len;
	}
	return 0;
}

int dequeue_data(struct data_queue *queue, void *p_buf, size_t len)
{
	char *buf = (char *)p_buf;
	if(queue == NULL)
	{
		return -1;
	}
	if(len > QUEUE_DATA_SIZE)
	{
    	log_error("read size greater than buffer.");
		return -1;
	}

	if(len <= atomic_load(&queue->buffer_head) - atomic_load(&queue->buffer_tail))
	{
		if(len <= QUEUE_DATA_SIZE - (atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE))
		{
			memcpy(buf, &queue->buffer_queue[atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE], len);
			atomic_fetch_add(&queue->buffer_tail, len);
		}else{
			size_t len_tmp = QUEUE_DATA_SIZE - (atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE);
			memcpy(buf, &queue->buffer_queue[atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE], len_tmp);
			atomic_fetch_add(&queue->buffer_tail, len_tmp);
			buf += len_tmp;
			memcpy(buf, &queue->buffer_queue[atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE], len - len_tmp);
			atomic_fetch_add(&queue->buffer_tail, (len - len_tmp));
		}

		return len;
	}
	return 0;
}

int read_data(struct data_queue *queue, void *p_buf, size_t len)
{
	char *buf = (char *)p_buf;
	if(queue == NULL)
	{
		return -1;
	}
	if(len > QUEUE_DATA_SIZE)
	{
    	log_error("read size greater than buffer.");
		return -1;
	}

	if(len <= atomic_load(&queue->buffer_head) - atomic_load(&queue->buffer_tail))
	{
		if(len <= QUEUE_DATA_SIZE - (atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE))
		{
			memcpy(buf, &queue->buffer_queue[atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE], len);
		}else{
			size_t len_tmp = QUEUE_DATA_SIZE - (atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE);
			memcpy(buf, &queue->buffer_queue[atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE], len_tmp);
			memcpy(buf + len_tmp, &queue->buffer_queue[(queue->buffer_tail + len_tmp) % QUEUE_DATA_SIZE], len - len_tmp);
		}

		return len;
	}
	return 0;
}

int queue_tail_point_forward(struct data_queue *queue, size_t len)
{
	if(queue == NULL)
	{
		return -1;
	}
	if(len > QUEUE_DATA_SIZE)
	{
    	log_error("[%s] read size greater than buffer.", __func__);
		return -1;
	}

	if(len <= atomic_load(&queue->buffer_head) - atomic_load(&queue->buffer_tail))
	{
		atomic_fetch_add(&queue->buffer_tail, len);
		return len;
	}
	return 0;
}

int queue_head_point_forward(struct data_queue *queue, size_t len)
{
	if(queue == NULL)
	{
		return -1;
	}
	if(len > QUEUE_DATA_SIZE)
	{
    	log_error("[%s] write size greater than buffer.", __func__);
		return -1;
	}

	if(len <= atomic_load(&queue->buffer_tail) + QUEUE_DATA_SIZE - atomic_load(&queue->buffer_head))
	{
		atomic_fetch_add(&queue->buffer_head, len);
		return len;
	}
	return 0;
}

char *queue_get_write_ptr(struct data_queue *queue)
{
	if(queue == NULL)
	{
		return NULL;
	}
	return queue->buffer_queue + (atomic_load(&queue->buffer_head) % QUEUE_DATA_SIZE);
}

char *queue_get_read_ptr(struct data_queue *queue)
{
	if(queue == NULL)
	{
		return NULL;
	}
	return queue->buffer_queue + (atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE);
}

int queue_get_free_count(struct data_queue *queue)
{
	if(queue == NULL)
		return -1;
	return QUEUE_DATA_SIZE + atomic_load(&queue->buffer_tail) - atomic_load(&queue->buffer_head);
}

int get_queue_data_count(struct data_queue *queue)
{
	if(queue == NULL)
		return -1;
	return atomic_load(&queue->buffer_head) - atomic_load(&queue->buffer_tail);
}

int queue_get_end_data_count(struct data_queue *queue)
{
	int end_data_count;
	int data_count;
	if(queue == NULL)
	{
		return -1;
	}
	data_count = get_queue_data_count(queue);
	end_data_count = QUEUE_DATA_SIZE - (atomic_load(&queue->buffer_tail) % QUEUE_DATA_SIZE);
	return end_data_count < data_count ? end_data_count : data_count;
}

int queue_get_end_free_count(struct data_queue *queue)
{
	int direct_space;
	int available_space;
	if(queue == NULL)
	{
		return -1;
	}
	available_space = queue_get_free_count(queue);
	direct_space = QUEUE_DATA_SIZE - (atomic_load(&queue->buffer_head) % QUEUE_DATA_SIZE);

	return available_space < direct_space ? available_space : direct_space;
}