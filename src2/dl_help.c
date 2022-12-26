#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>

void* load_lib(const char *path)
{
	if(access(path, F_OK))
		return NULL;

	return dlopen(path, RTLD_LAZY);
}

void* load_lib_data(const char *path, const char *key)
{
	void* obj = NULL;
	void* handle = load_lib(path);
	if(handle)
	{
		obj = dlsym(handle, key);
	}
	return obj;
}

int release_lib(void* handle)
{
	return dlclose(handle);
}