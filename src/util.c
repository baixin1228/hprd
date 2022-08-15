#include <sys/time.h>

#include "util.h"

static __attribute__((unused)) uint64_t get_time_ns()
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL))
        return 0;
    return tv.tv_usec + tv.tv_sec * 1000000;
}