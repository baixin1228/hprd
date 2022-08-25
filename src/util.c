#include <sys/time.h>
#include <string.h>

#include "util.h"

static __attribute__((unused)) uint64_t get_time_ns()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL))
        return 0;
    return tv.tv_usec + tv.tv_sec * 1000000;
}

inline int smp_frame_copy(uint8_t *to, size_t to_h_stride, uint8_t *from, size_t from_h_stride, size_t width, size_t height)
{
	if(width > to_h_stride)
		return -1;
	if(width > from_h_stride)
		return -1;

	#pragma omp parallel for
	for (int i = 0; i < height; ++i)
	{
		memcpy(to + to_h_stride * i, from + from_h_stride * i, width);
	}
	return 0;
}