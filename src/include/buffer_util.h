#ifndef __MINI_RING_QUEUE_H__
#define __MINI_RING_QUEUE_H__
#include <stdint.h>
#include "common.h"
#include "buffer_pool.h"

struct dev_id_queue
{
    struct buf
    {
        bool is_used;
        bool is_free;
    } bufs[MAX_BUFFER_COUNT];
    uint64_t head_buf_id;
    uint64_t tail_buf_id;
    uint16_t max_idx;
};

int dev_id_queue_get_buf(struct dev_id_queue *queue);
int dev_id_queue_put_buf(struct dev_id_queue *queue, int buf_id);

int dev_id_queue_get_id(struct dev_id_queue *queue);
int dev_id_queue_sub_id(struct dev_id_queue *queue);

int dev_id_queue_set_status(struct dev_id_queue *queue, int id, bool is_used);
#endif  /* __MINI_RING_QUEUE_H__ */