#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <execinfo.h>

#include "util.h"

static void _show_file_line(char addr[])
{
	char cmd[256];
	char line[256];
	char addrline[32] = {0,};
	char *str1, *str2;
	FILE *file;

	str1 = strchr(addr, '[');
	str2 = strchr(addr, ']');
	if (str1 == NULL || str2 == NULL)
		return;
	memcpy(addrline, str1 + 1, str2 - str1);
	snprintf(cmd, sizeof(cmd), "addr2line -Cfe /proc/%d/exe %s ", getpid(), addrline);
	file = popen(cmd, "r");
	if (NULL != fgets(line, 256, file))
		printf("%s", line);
	pclose(file);
}

void print_stack(char *sig)
{
	void *array[32];
	size_t size;
	char **strings;
	int i;

	printf("%s\n", sig);
	size = backtrace (array, 32);
	strings = backtrace_symbols (array, size);
	if (NULL == strings) {
		printf("backtrace_symbols\n");
		return ;
	}

	for (i = 0; i < size; i++) {
		printf("%s", strings[i]);
		_show_file_line(strings[i]);
	}

	free(strings);
}

void _printf_stack(int dunno)
{
	char *signal_str = "";
	char dunno_str[10] = {0};
	sprintf(dunno_str, "%d", dunno);
	switch (dunno) {
	case SIGHUP:
		signal_str = "SIGHUP(1)";
		break;
	case SIGINT:
		signal_str = "SIGINT(2:CTRL_C)"; //CTRL_C
		break;
	case SIGQUIT:
		signal_str = "SIGQUIT(3)";
		break;
	case SIGILL:
		signal_str = "非法指令";
		print_stack(signal_str);
		break;
	case SIGABRT:
		signal_str = "栈越界、double free";
		print_stack(signal_str);
		break;
	case SIGBUS:
		signal_str = "总线错误，内存不对齐";
		print_stack(signal_str);
		break;
	case SIGFPE:
		signal_str = "浮点异常";
		print_stack(signal_str);
		break;
	case SIGSEGV:
		signal_str = "段错误：";
		print_stack(signal_str);
		break;
	case SIGPIPE:
		signal_str = "scoket err";
		print_stack(signal_str);
		break;
	case SIGSTKFLT:
	    signal_str = "超出内存";
	    print_stack(signal_str);
	    break;
	default:
		signal_str = "OTHER";
		break;
	}
}

void _on_exit(void)
{
}

void _signal_crash_handler(int sig)
{
	_printf_stack(sig);
	exit(-1);
}

void _signal_exit_handler(int sig)
{
	exit(0);
}

void debug_info_regist()
{
	static bool is_regist = false;
	if(is_regist)
		return;

	is_regist = true;

	atexit(_on_exit);

	signal(SIGHUP, _signal_exit_handler);        //1.终端退出信号
	signal(SIGINT, _signal_exit_handler);        //2.ctrl+c
	signal(SIGQUIT, _signal_exit_handler);        //3."ctrl+\"
	signal(SIGILL, _signal_crash_handler);        //4.非法指令
	signal(SIGABRT,
		   _signal_crash_handler);        //6.SIGABRT，由调用abort函数产生，进程非正常退出
	signal(SIGBUS,
		   _signal_crash_handler);        //7.总线错误，访问不存在的总线地址
	signal(SIGFPE,
		   _signal_crash_handler);        //8.SIGFPE，数学相关的异常，如被0除，浮点溢出，等等
	signal(SIGKILL, _signal_exit_handler);        //9.kill -9
	signal(SIGSEGV,
		   _signal_crash_handler);        //11.没有访问权限，数组下标越界
	signal(SIGPIPE, _signal_crash_handler);        //13.管道错误
	signal(SIGALRM, _signal_exit_handler);        //14.alarm

	signal(SIGTERM, _signal_exit_handler);        //15.kill
	signal(SIGSTKFLT, _signal_crash_handler);    //16.Stack fault
}

uint64_t get_time_us(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return time.tv_sec * 1000000 + time.tv_usec;
}

uint64_t get_time_ms(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return time.tv_sec * 1000 + time.tv_usec / 1000;
}