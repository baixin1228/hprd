#ifndef __RTNP_CORE_H__
#define __RTNP_CORE_H__

#include <stdint.h>
#include <pthread.h>

struct data_package {
	uint32_t package_id;
	uint8_t group_id;
	uint32_t data_start;
	uint32_t data_pos;
};

#define PKG_LEN 512
#define DATA_BUF_LEN (1024 * 1024)
struct rtnp {
	uint32_t window_size;
	pthread_spinlock_t lock;
	pthread_spinlock_t write_lock;
	uint32_t pkg_arr_len;
	struct data_package *pkg_arr;
	uint32_t data_arr_len;
	char *data_arr;
	/* write pointer */
	uint32_t head;
	/* read pointer */
	uint32_t tail;
};

#endif