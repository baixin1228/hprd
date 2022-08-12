#ifndef __MODULE_H__
#define __MODULE_H__
#include "ld_help.h"

struct module_data
{
	void *priv;
	void *ops;
	int (*on_event)();
};

#define FRAMEBUFFER_INPUT_DEV  0
#define FRAMEBUFFER_OUTPUT_DEV  1

#define DEVICE_PRIO_HEIGHT   0
#define DEVICE_PRIO_MIDDLE   1
#define DEVICE_PRIO_LOW   2

#define _REGISTE_MODULE_DEV(dev, class, prio)						\
void *_##dev##registe(void)											\
{																	\
	return (void *)&dev;											\
}																	\
MODULE_INIT(_##dev##registe, class, prio)

#define REGISTE_MODULE_DEV(dev, class, prio) _REGISTE_MODULE_DEV(dev, class, prio)

#endif