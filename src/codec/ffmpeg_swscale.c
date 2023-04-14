#include <libswscale/swscale.h>

#include "util.h"
#include "queue.h"
#include "buffer_pool.h"

struct ffmpeg_swscale_data {
	struct SwsContext *sws_ctx;

	int buf_id;
};

static int ffmpeg_swsacle_init(struct encodec_object *obj) {
	struct ffmpeg_swscale_data *sws_data;
	sws_data = calloc(1, sizeof(*sws_data));

	if(!sws_data) {
		log_error("calloc fail, check free memery.");
		return -1;
	}
	sws_data->buf_id = -1;

	obj->priv = sws_data;
}

static int ffmpeg_swsacle_set_info(struct encodec_object *obj,
								   GHashTable *info) {
	struct ffmpeg_swscale_data *sws_data = obj->priv;

	uint32_t in_width = *(uint32_t *)g_hash_table_lookup(info, "in_width");
	uint32_t in_height = *(uint32_t *)g_hash_table_lookup(info, "in_height");
	uint32_t in_format = *(uint32_t *)g_hash_table_lookup(info, "in_format");
	uint32_t out_width = *(uint32_t *)g_hash_table_lookup(info, "out_width");
	uint32_t out_height = *(uint32_t *)g_hash_table_lookup(info, "out_height");
	uint32_t out_format = *(uint32_t *)g_hash_table_lookup(info, "out_format");
	in_format = fb_fmt_to_av_fmt(in_format);
	out_format = fb_fmt_to_av_fmt(out_format);

	sws_data->sws_ctx = sws_getContext(in_width, in_height, in_format,
									   out_width, out_height, out_format, SWS_POINT, NULL, NULL, NULL);
}

static int ffmpeg_swsacle_put_puffer(struct convert_object *obj, int buf_id) {
	int new_buf_id;
	struct raw_buffer *in_buffer;
	struct raw_buffer *out_buffer;
	struct ffmpeg_swscale_data *sws_data = obj->priv;

	new_buf_id = get_fb(obj->buf_pool);
	if(new_buf_id < 0)
	{
		log_error("get_fb fail.");
		exit(-1);
	}

	in_buffer = get_raw_buffer(obj->buf_pool, buf_id);
	out_buffer = get_raw_buffer(obj->buf_pool, new_buf_id);

	int inlinesize[4] = {in_buffer->size / in_buffer->height, 0, 0, 0}; 
	int outlinesize[4] = {out_width, out_width/2, out_width/2, 0};

	sws_scale(sws_data->sws_ctx, in_buffer->ptrs, inlinesize, 0, in_height,
		out_buffer->ptrs, outlinesize);

	put_fb(obj->buf_pool, buf_id);
	sws_data->buf_id = new_buf_id;
	return 0;
}

static int ffmpeg_swsacle_get_fb(struct convert_object *obj) {
	struct ffmpeg_swscale_data *sws_data = obj->priv;
	return sws_data->buf_id;
}

static int ffmpeg_swsacle_release(struct convert_object *obj) {
	struct ffmpeg_swscale_data *sws_data = obj->priv;
	sws_freeContext(sws_data->sws_ctx);
}