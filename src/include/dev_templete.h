#ifndef __DEV_TEMPLETE_H__
#define __DEV_TEMPLETE_H__

#include <glib.h>
#include "util.h"

#define DEV_GET_INFO(__obj, __obj_ops)				\
int __obj##_get_info(struct __obj##_object *obj, GHashTable *fb_info)\
{													\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->get_info)						\
			return dev_ops->get_info(obj, fb_info); \
		else 										\
			log_error(#__obj" dev not find func:get_info\n");\
	}												\
													\
	return -1;										\
}

#define DEV_SET_INFO(__obj, __obj_ops)				\
int __obj##_set_info(struct __obj##_object *obj, GHashTable *fb_info)\
{													\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->set_info)						\
			return dev_ops->set_info(obj, fb_info); \
		else 										\
			log_error(#__obj" dev not find func:set_info\n");\
	}												\
													\
	return -1;										\
}

#define DEV_FUNC_2(__obj, __func)					\
int __obj##_##__func(struct __obj##_object *obj, int p1)\
{													\
	struct __obj##_ops *dev_ops;					\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj##_ops *)obj->ops;		\
	if(dev_ops)										\
	{												\
		if(dev_ops->__func)							\
			return dev_ops->__func(obj, p1);	\
		else 										\
			log_error(#__obj" dev not find func:" #__func "\n");\
	}												\
													\
	return -1;										\
}

#define DEV_FUNC_1(__obj, __func)					\
int __obj##_##__func(struct __obj##_object *obj)		\
{													\
	struct __obj##_ops *dev_ops;					\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj##_ops *)obj->ops;		\
	if(dev_ops)										\
	{												\
		if(dev_ops->__func)							\
			return dev_ops->__func(obj);			\
		else 										\
			log_error(#__obj" dev not find func:" #__func "\n");\
	}												\
													\
	return -1;										\
}

#define DEV_RESIZE(__obj, __obj_ops)				\
int __obj##_resize(struct __obj##_object *obj,		\
	uint32_t width, uint32_t height)				\
{													\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->resize)						\
			return dev_ops->resize(obj, width, height);\
		else 										\
			log_error(#__obj" dev not find func:resize\n");\
	}												\
													\
	return -1;										\
}

#define DEV_RELEASE(__obj, __obj_ops)				\
int __obj##_release(struct __obj##_object *obj)		\
{													\
	int ret;										\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->release)						\
		{											\
			ret = dev_ops->release(obj);			\
			if(ret == 0)							\
			{										\
				free(obj);							\
			}										\
		}else 										\
			log_error(#__obj" dev not find func:release\n");\
	}												\
													\
	return -1;										\
}
#endif
