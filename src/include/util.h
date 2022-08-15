#ifndef __UTIL_H__
#define __UTIL_H__
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define ALIGN16(x)  ((x+15)&~15)
#define ALIGN32(x)  ((x+31)&~31)
#define ALIGN64(x)  ((x+63)&~63)
#define ALIGN128(x)  ((x+127)&~127)

#define MIN(a, b) ((a)>(b)?(b):(a))
#define MAX(a, b) ((a)>(b)?(a):(b))

enum ENGINE_LOG_LEVEL
{
	ENGINE_LOG_DEBUG,
	ENGINE_LOG_INFO,
	ENGINE_LOG_WARNING,
	ENGINE_LOG_ERROR
};

static inline void log_info(const char *fmt, ...)
{
	char fmt_buffer[1024];
	sprintf(fmt_buffer, "\033[0m\033[1;34mInfo:%s\033[0m\n", fmt);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vfprintf(stdout, fmt_buffer, va);
	va_end(va);
}

static inline void log_warning(const char *fmt, ...)
{
	char fmt_buffer[1024];
	sprintf(fmt_buffer, "\033[0m\033[1;33mWarning:%s\033[0m\n", fmt);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vfprintf(stderr, fmt_buffer, va);
	va_end(va);
}

static inline void log_error(const char *fmt, ...)
{
	char fmt_buffer[1024];
	sprintf(fmt_buffer, "\033[0m\033[1;31mError:%s\033[0m\n", fmt);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vfprintf(stderr, fmt_buffer, va);
	va_end(va);
}

static inline void _func_info(const char *fmt, char *file, char *func, uint32_t line, ...)
{
	char fmt_buffer[1024];
	char str_buffer[1024];
	sprintf(fmt_buffer, "%s%s:%d <%s>\n", fmt, file, line, func);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vsprintf(str_buffer, fmt_buffer, va);
	va_end(va);
	log_info(str_buffer);
}

static inline void _func_warning(const char *fmt, char *file, char *func, uint32_t line, ...)
{
	char fmt_buffer[1024];
	char str_buffer[1024];
	sprintf(fmt_buffer, "%s%s:%d <%s>\n", fmt, file, line, func);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vsprintf(str_buffer, fmt_buffer, va);
	va_end(va);
	log_warning(str_buffer);
}

static inline void _func_error(const char *fmt, char *file, const char *func, uint32_t line, ...)
{
	char fmt_buffer[1024];
	char str_buffer[1024];
	sprintf(fmt_buffer, "%s%s:%d <%s>\n", fmt, file, line, func);
	va_list va;
	#pragma GCC diagnostic ignored "-Wvarargs"
	va_start(va, fmt_buffer);
	vsprintf(str_buffer, fmt_buffer, va);
	va_end(va);
	log_error(str_buffer);
}

#define func_info(fmt, arg...) _func_info((fmt), __FILE__, __func__, __LINE__, ##arg)
#define func_warning(fmt, arg...) _func_warning((fmt), __FILE__, __func__, __LINE__, ##arg)
#define func_error(fmt, arg...) _func_error((fmt), __FILE__, __func__, __LINE__, ##arg)
#endif