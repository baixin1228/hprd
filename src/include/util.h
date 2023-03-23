#ifndef __UTIL_H__
#define __UTIL_H__
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#define ALIGN4(x)	((x+3)&~3)
#define ALIGN8(x)	((x+7)&~7)
#define ALIGN16(x)  ((x+15)&~15)
#define ALIGN32(x)  ((x+31)&~31)
#define ALIGN64(x)	((x+63)&~63)
#define ALIGN128(x)	((x+127)&~127)

static inline void log_info(const char *fmt, ...)
{
	char fmt_buffer[1024];
	sprintf(fmt_buffer, "[\033[0m\033[1;34mInfo\033[0m] %s\n", fmt);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vfprintf(stdout, fmt_buffer, va);
	va_end(va);
	fflush(stdout);
}

static inline void log_warning(const char *fmt, ...)
{
	char fmt_buffer[1024];
	sprintf(fmt_buffer, "[\033[0m\033[1;33mWarn\033[0m] %s\n", fmt);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vfprintf(stderr, fmt_buffer, va);
	va_end(va);
	fflush(stderr);
}

static inline void log_error(const char *fmt, ...)
{
	char fmt_buffer[1024];
	sprintf(fmt_buffer, "[\033[0m\033[1;31mError\033[0m] %s\n", fmt);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vfprintf(stderr, fmt_buffer, va);
	va_end(va);
	fflush(stderr);
}

static inline void log_perr(const char *fmt, ...)
{
	char fmt_buffer[1024];
	sprintf(fmt_buffer, "[\033[0m\033[1;31mError\033[0m] %s\nerr_no:%d %s\n",
		fmt, errno, strerror (errno));
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vfprintf(stderr, fmt_buffer, va);
	va_end(va);
	fflush(stderr);
}

void print_stack(char *sig);
void debug_info_regist();
uint64_t get_time_ms(void);
uint64_t get_time_us(void);
#endif