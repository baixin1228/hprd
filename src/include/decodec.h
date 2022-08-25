#ifndef __DECODEC_H__
#define __DECODEC_H__
#include "buffer.h"
#include "module.h"
#include "codec.h"

struct decodec_info
{
	enum STREAM_FORMAT stream_fmt;
};

enum PUSH_STATUS
{
	PUSH_OK,
	PUSH_FAIL,
	PUSH_UNKNOW,
	PUSH_BUFFER_FULL,
};

struct decodec_ops
{
	char * name;
	int (* init)(struct module_data *dev, struct decodec_info enc_info);
	enum PUSH_STATUS (* push_pkt)(struct module_data *dev, struct common_buffer *buffer);
	struct common_buffer *(* get_fb)(struct module_data *dev);
	int (* release)(struct module_data *dev);
};

#define REGISTE_DECODEC_DEV(dev, prio) REGISTE_MODULE_DEV(dev, DECODEC_DEV, prio)

struct module_data *decodec_init_dev(struct decodec_info enc_info);
enum PUSH_STATUS decodec_push_pkt(struct module_data *dev, struct common_buffer *buffer);
struct common_buffer *decodec_get_fb(struct module_data *dev);
int decodec_release(struct module_data *dev);
#endif