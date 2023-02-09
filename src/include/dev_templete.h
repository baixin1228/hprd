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
			return dev_ops->get_info(obj, fb_info);\
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
			log_error(#__obj" dev not find func:map_buffer\n");\
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
#endif