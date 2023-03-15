static inline void _show_file_line(char addr[])
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

static inline void out_stack(char *sig)
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