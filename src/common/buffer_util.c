#include "buffer_util.h"

int dev_id_queue_get_buf(struct dev_id_queue *queue) {
	int ret = -1;

	if(queue->head_buf_id >= queue->tail_buf_id)
	{
		return -1;
	}

	while(!queue->bufs[queue->head_buf_id % queue->max_idx].is_used) {
		queue->head_buf_id++;
	}

	if(queue->bufs[queue->head_buf_id % queue->max_idx].is_free) {
		queue->bufs[queue->head_buf_id % queue->max_idx].is_free = false;
		ret = queue->head_buf_id % queue->max_idx;
		queue->head_buf_id++;
	}

	return ret;
}

int dev_id_queue_put_buf(struct dev_id_queue *queue, int buf_id)
{
	int ret = -1;
	if(buf_id > 0 && buf_id < MAX_BUFFER_COUNT)
	{
		if(!queue->bufs[buf_id].is_free) {
			queue->bufs[buf_id].is_free = true;
			ret = 0;
		}
	}

	return ret;
}

int dev_id_queue_get_id(struct dev_id_queue *queue)
{
	int ret = -1;

	if(queue->head_buf_id + queue->max_idx <= queue->tail_buf_id)
		return -1;

	while(!queue->bufs[queue->tail_buf_id % queue->max_idx].is_used) {
		queue->tail_buf_id++;
	}

	if(queue->bufs[queue->tail_buf_id % queue->max_idx].is_free) {
		ret = queue->tail_buf_id % queue->max_idx;
	}

	return ret;
}

int dev_id_queue_sub_id(struct dev_id_queue *queue)
{
	queue->tail_buf_id++;
	return 0;
}

int dev_id_queue_set_status(struct dev_id_queue *queue, int buf_id, bool is_used) {
	if(buf_id > 0 && buf_id < MAX_BUFFER_COUNT)
	{
		queue->bufs[buf_id].is_used = is_used;
		queue->bufs[buf_id].is_free = is_used;
		if(queue->max_idx < buf_id)
			queue->max_idx = buf_id;

		return 0;
	}

	return -1;
}
