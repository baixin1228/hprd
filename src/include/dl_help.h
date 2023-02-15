#ifndef __DL_HELP_H__
#define __DL_HELP_H__

void* load_lib(const char *patch);
void* load_lib_data(const char *patch, const char *key);
int release_lib(void* handle);

#endif