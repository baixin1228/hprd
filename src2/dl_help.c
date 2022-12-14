#include <stdio.h>
#include <dlfcn.h>

void* load_lib(const char *patch)
{
	void* handle = dlopen(patch, RTLD_LAZY);
	return handle;
}

void* load_lib_data(const char *patch, const char *key)
{
	void* handle = dlopen(patch, RTLD_LAZY);
	void* obj = dlsym(handle, key);
	return obj;
}

int release_lib(void* handle)
{
	return dlclose(handle);
}