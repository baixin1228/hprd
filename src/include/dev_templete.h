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

#define DEV_MAP_FB(__obj, __obj_ops)				\
int __obj##_map_fb(struct __obj##_object *obj, int buf_id)\
{													\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->map_buffer)						\
			return dev_ops->map_buffer(obj, buf_id);\
		else 										\
			log_error(#__obj" dev not find func:map_buffer\n");\
	}												\
													\
	return -1;										\
}

#define DEV_UNMAP_FB(__obj, __obj_ops)				\
int __obj##_unmap_fb(struct __obj##_object *obj, int buf_id)\
{													\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->unmap_buffer)					\
			return dev_ops->unmap_buffer(obj, buf_id);\
		else 										\
			log_error(#__obj" dev not find func:unmap_buffer\n");\
	}												\
													\
	return -1;										\
}

#define DEV_GET_FB(__obj, __obj_ops)				\
int __obj##_get_fb(struct __obj##_object *obj)		\
{													\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->get_buffer)						\
			return dev_ops->get_buffer(obj);		\
		else 										\
			log_error(#__obj" dev not find func:get_buffer\n");\
	}												\
													\
	return -1;										\
}

#define DEV_PUT_FB(__obj, __obj_ops)				\
int __obj##_put_fb(struct __obj##_object *obj, int buf_id)\
{													\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->put_buffer)						\
			return dev_ops->put_buffer(obj, buf_id);\
		else 										\
			log_error(#__obj" dev not find func:put_buffer\n");\
	}												\
													\
	return -1;										\
}

#define DEV_FORCE_I(__obj, __obj_ops)				\
int __obj##_force_i(struct __obj##_object *obj)		\
{													\
	struct __obj_ops *dev_ops;						\
													\
	if(!obj)										\
		return -1;									\
													\
	dev_ops = (struct __obj_ops *)obj->ops;			\
	if(dev_ops)										\
	{												\
		if(dev_ops->force_i)						\
			return dev_ops->force_i(obj);			\
		else 										\
			log_error(#__obj" dev not find func:force_i\n");\
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
				release_lib(obj->ops);				\
				free(obj);							\
			}										\
		}else 										\
			log_error(#__obj" dev not find func:release\n");\
	}												\
													\
	return -1;										\
}

#define GET_DEV_OPS(__type, __devs, __dev_name)		\
({													\
	int idx;										\
	struct __type *dev_ops = NULL;					\
													\
	if(__dev_name != NULL)							\
	{												\
		for (int i = 0; i < sizeof(__devs) / sizeof(struct __type*); ++i)\
		{											\
			if(strcmp(__dev_name, __devs[i]->name) == 0)\
				idx = i;							\
		}											\
		dev_ops = __devs[idx];						\
	}												\
	dev_ops; 										\
})

#endif
