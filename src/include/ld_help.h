#ifndef __LD_HELP_H__
#define __LD_HELP_H__

#define __RO_SECTION_DECLARE(order_1, order_2, order_3, sym) \
__attribute__((__used__, aligned(8), section(".rodata1." #order_1 "." #order_2 "." #order_3 "." #sym)))

struct __module_item {
	void *(*handler)(void);
};

#define _REGISTE_MODULE_ITEM(class_no, handler, order, order2) \
static const struct __module_item __rodata1##_##class_no##_##order##_##order2##_##handler \
__RO_SECTION_DECLARE(class_no, order, order2, handler) = {handler}

#define _MODULE_INIT(handler, module_no, order) \
_REGISTE_MODULE_ITEM(module_no, handler, 1, order)

#define MODULE_INIT(handler, module_no, order) _MODULE_INIT(handler, module_no, order)

void *null_func(void);
#define _MODULE_BOUNDARY(handler, module_no) \
_REGISTE_MODULE_ITEM(module_no, handler, 0, 0); \
_REGISTE_MODULE_ITEM(module_no, handler, 2, 0)

#define MODULE_BOUNDARY(handler, module_no) _MODULE_BOUNDARY(handler, module_no)

#define _FOREACH_ITEM(_var, handler, class_no) \
for (_var = &__rodata1_##class_no##_0_0_##handler, ++_var; \
	_var < &__rodata1_##class_no##_2_0_##handler; \
	_var++)

#define FOREACH_ITEM(_var, handler, class_no) _FOREACH_ITEM(_var, handler, class_no)

#endif