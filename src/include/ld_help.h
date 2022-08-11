#ifndef __LD_HELP_H__
#define __LD_HELP_H__

#define __RO_SECTION_DECLARE(order_1, order_2, order_3, sym) \
__attribute__((__used__, aligned(8), section(".rodata1." #order_1 "." #order_2 "." #order_3 "." #sym))) \

struct __module_item {
	void (*handler)(void);
};

#define _REGISTE_MODULE_ITEM(class_no, handler, order, order2) \
static const volatile struct __module_item __rodata1##_##class_no##_##order##_##order2##_##handler \
__RO_SECTION_DECLARE(order, class_no, order2, handler) = {handler}

#define MODULE_INIT(handler, module_no, order) \
enum { _Sys_module_##handler = order*module_no }; \
_REGISTE_MODULE_ITEM(module_no, handler, 1, order)

void null_func(void);

#define MODULE_BOUNDARY(module_no) \
enum { _Sys_module_##handler = module_no }; \
_REGISTE_MODULE_ITEM(module_no, null_func, 0, 0); \
_REGISTE_MODULE_ITEM(module_no, null_func, 2, 0)

#define FOREACH_ITEM(_var, class_no) \
for (_var = &__module_item __mods##_##class_no##_##NULL_0_0, ++_var; \
	_var < &__module_item __mods##_##class_no##_##NULL_2_0; \
	_var++)


#endif