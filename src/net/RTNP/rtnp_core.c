#include <unistd.h>

#include "rtnp_core.h"

int rtnp_init(struct rtnp *self, uint32_t pkg_len, uint32_t data_buf_len)
{
	self->pkg_arr_len = pkg_len;
	self->pkg_arr = malloc(self->pkg_arr_len * sizeof(*self->pkg_arr));
	if(self->pkg_arr == NULL)
	{
		perror("malloc");
		exit(-1);
	}
	self->data_arr_len = data_buf_len;
	self->data_arr = malloc(self->data_arr_len);
	if(self->data_arr == NULL)
	{
		perror("malloc");
		exit(-1);
	}
	pthread_spin_init(&self->write_lock);
	pthread_spin_init(&self->lock);
}

int send_package(struct rtnp *self, char *data, size_t len)
{
	int let_space;
	if(len > self->data_arr)
	{
		fprintf(2, "[%s] %s", __func__, "data len is ou of limit.");
		return -1;
	}
	pthread_spin_lock(&self->write_lock);
	while(self->data_arr_len - (self->head - self->tail) < len)
		usleep(1);

	let_space = self->data_arr_len - self->head % self->data_arr_len;
	if(len <= let_space)
	{
		memcpy(data, self->data_arr[self->head % self->data_arr_len], len);
		self->head += len;
	}else{
		memcpy(data, self->data_arr[self->head % self->data_arr_len], let_space);
		self->head += let_space;
		memcpy(data, self->data_arr[self->head % self->data_arr_len], (len - let_space));
		self->head += (len - let_space);
	}
	pthread_spin_unlock(&self->write_lock);
}