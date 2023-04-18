#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

#include "ikcp.h"

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <windows.h>
#elif !defined(__unix)
#define __unix
#endif

#ifdef __unix
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#endif

/* get system time */
void itimeofday(long *sec, long *usec);
/* get clock in millisecond 64 */
IINT64 iclock64(void);
IUINT32 iclock();
/* sleep in millisecond */
void isleep(unsigned long millisecond);