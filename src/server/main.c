#include <stdlib.h>

#include "module.h"
#include "util.h"

volatile void null_func(void) {}

MODULE_BOUNDARY(FRAMEBUFFER_INPUT_DEV);

int main()
{
	// const struct sysinit_item *item;
	// FOREACH_ITEM(item, sysinit)
	// 	item->handler();
	return 0;
}