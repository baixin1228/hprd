#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <stdatomic.h>
#include <drm_fourcc.h>
#include <xf86drmMode.h>
#include <rga/im2d.h>
#include <rga/RgaApi.h>
#include <rga/rga.h>
#include "gl/gl_help.h"
#include "codec/ffmpeg_mult_dec.h"
#include "util.h"

#define SLEEP_DEFAULT_US 10000
#define PROBE_SIZE 1024 * 1024

void show_all_codec()
{
	char *info = (char *)malloc(40000);
	memset(info, 0, 40000);
	
	av_register_all();
 
	AVCodec *c_temp = av_codec_next(NULL);
 
	while (c_temp != NULL)
	{
		if (c_temp->decode != NULL)
		{
			strcat(info, "[Decode]");
		}
		else
		{
			strcat(info, "[Encode]");
		}
		switch (c_temp->type)
		{
		case AVMEDIA_TYPE_VIDEO:
			strcat(info, "[Video]");
			break;
		case AVMEDIA_TYPE_AUDIO:
			strcat(info, "[Audeo]");
			break;
		default:
			strcat(info, "[Other]");
			break;
		}
		sprintf(info, "%s %10s", info, c_temp->name);
		c_temp = c_temp->next;
	}
	puts(info);
	free(info);
	return 0;
}

int fill_iobuffer(void * opaque, uint8_t *buf, int bufsize){
	struct mini_codec* code_ctx = (struct mini_codec*)opaque;
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	int ret;
	uint32_t counter = 0;

	// printf("%s %d", __func__, bufsize);
IO_RETRY:
	ret = read_queue_data(&data->queue, buf, bufsize);
	if(ret > 0)
		return ret;
	if(ret == QUEUE_NO_DATA)
	{
		counter++;
		usleep(SLEEP_DEFAULT_US);
		if(counter > 100)
		{
			printf("fill_iobuffer:NO DATA");
			counter = 0;
		}

		if(!data->pkt_eos_flag && !code_ctx->exit_flag)
		{
			goto IO_RETRY;
		}
	}

	return ret;
}

int _on_av_frame(struct mini_codec* code_ctx, struct gl_obj *gl)
{
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	AVCodecContext  *av_codec_ctx = data->av_codec_ctx;
	AVFrame *frame = data->av_frame;
	uint32_t dec_frame_idx = data->dec_frame % FRAME_COUNT;
	struct user_drm_buffer *drm_buffer;
	while(atomic_load(&data->frame_status[dec_frame_idx]))
	{
		if(code_ctx->exit_flag || data->pkt_eos_flag)
		{
			return 0;
		}
		if(data->print_counter > 100)
		{
			log_warning("waiting to frame randered:%d next show:%d", data->dec_frame, data->show_frame);
			data->print_counter = 0;
		}
		data->print_counter++;
		usleep(SLEEP_DEFAULT_US);
	}
	data->print_counter = 0;

	drm_buffer = gl->surfaces[dec_frame_idx].drm_buffer;

	switch(frame->format)
	{
		case AV_PIX_FMT_YUV420P:
		if(code_ctx->use_rga)
		{
			int ret;
			rga_buffer_t 	src;
			rga_buffer_t 	dst;
    		dst = wrapbuffer_fd(drm_buffer->fd, drm_buffer->width, drm_buffer->height ,RK_FORMAT_YCbCr_420_P);
			src = wrapbuffer_virtualaddr(frame->data[0], drm_buffer->width, drm_buffer->height ,RK_FORMAT_YCbCr_420_P);
            ret = imresize(src, dst);
            if(ret != IM_STATUS_SUCCESS)
            {
                printf("imresize fail.");
                printf("resizing .... %s", imStrError(ret));
                exit(-1);
            }
		}else if(data->use_sw)
		{
			char *frame_data[3];
			frame_data[0] = drm_buffer->ptr;
			frame_data[2] = drm_buffer->ptr + drm_buffer->h_stride * drm_buffer->height;
			frame_data[1] = drm_buffer->ptr + drm_buffer->h_stride * drm_buffer->height +
							drm_buffer->h_stride * drm_buffer->height / 4;
			int frame_linesize[3];
			frame_linesize[0] = drm_buffer->h_stride;
			frame_linesize[1] = drm_buffer->h_stride / 2;
			frame_linesize[2] = drm_buffer->h_stride / 2;
			sws_scale(data->img_convert_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0, frame->height, frame_data, frame_linesize);
		}else{
			_smp_frame_cpy(drm_buffer->ptr, 
			frame->linesize[0], 
			frame->data[0], 
			frame->linesize[0], 
			frame->linesize[0], 
			drm_buffer->height);
			_smp_frame_cpy(drm_buffer->ptr + frame->linesize[0] * drm_buffer->height,
				frame->linesize[0] / 2,
				frame->data[2], 
				frame->linesize[0] / 2, 
				frame->linesize[0] / 2, 
				drm_buffer->height / 2);
			_smp_frame_cpy(drm_buffer->ptr + frame->linesize[0] * drm_buffer->height +
				frame->linesize[0] * drm_buffer->height / 4,
				frame->linesize[0] / 2,
				frame->data[1],
				frame->linesize[0] / 2,
				frame->linesize[0] / 2,
				drm_buffer->height / 2);
		}
		break;
		case AV_PIX_FMT_RGB24:     ///< packed RGB 8:8:8, 24bpp, RGBRGB...
			_smp_frame_cpy(drm_buffer->ptr,
			frame->linesize[0],
			frame->data[0],
			frame->linesize[0],
			frame->linesize[0],
			drm_buffer->height * 3);
		break;
		case AV_PIX_FMT_NV12:      ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
			_smp_frame_cpy(drm_buffer->ptr,
			frame->linesize[0],
			frame->data[0],
			frame->linesize[0],
			frame->linesize[0],
			drm_buffer->height);
			_smp_frame_cpy(drm_buffer->ptr + frame->linesize[0] * drm_buffer->height,
					frame->linesize[0],
					frame->data[0],
					frame->linesize[0],
					frame->linesize[0],
					drm_buffer->height);
		break;
	}
	data->dec_frame++;
	atomic_store(&data->frame_status[dec_frame_idx], 1);
}

int _ffmpeg_bind_to_texture0(struct mini_codec* code_ctx)
{
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	struct display_engine *display_engine = code_ctx->display_engine;
	struct gl_obj *gl = display_engine->gl;
	struct user_drm_buffer *drm_buffer;

    if(!gl)
        return;
    if(!gl->is_init)
        return;

	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		drm_buffer = gl->surfaces[i].drm_buffer;
		if(bind_drm_buffer_to_texture0(gl, &gl->surfaces[i], drm_buffer) != 0)
		{
            line_error("bind_drm_buffer_to_texture0 fail. idx:%d", i);
            return -1;
		}
	}
    return 0;
}

int _reinit_shader(struct mini_codec* code_ctx)
{
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	struct display_engine *display_engine = code_ctx->display_engine;
	struct gl_obj *gl = display_engine->gl;
	AVCodecContext  *av_codec_ctx = data->av_codec_ctx;
	
    if(!gl)
        return;
    if(!display_engine->is_show)
        return;

	switch(av_codec_ctx->pix_fmt)
	{
		case AV_PIX_FMT_YUV420P:
			gl_init_shader(gl, DRM_FORMAT_YUV420);
			log_info("AV_PIX_FMT_YUV420P");
		break;
		case AV_PIX_FMT_RGB24:
			gl_init_shader(gl, DRM_FORMAT_RGB888);
			log_info("AV_PIX_FMT_RGB24");
		break;
		case AV_PIX_FMT_NV12:
			log_info("AV_PIX_FMT_NV12");
		break;
		default:
			log_info("AV_PIX_FMT_NULL");
		break;
	}
}

int mini_ffmpeg_reinit_shader(struct mini_codec* code_ctx)
{
    if(!code_ctx->has_status)
        return -1;

	_reinit_shader(code_ctx);
}

int _ffmpeg_alloc_buffer(struct mini_codec* code_ctx)
{
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	struct display_engine *display_engine = code_ctx->display_engine;
	struct gl_obj *gl = display_engine->gl;

	AVCodecContext  *av_codec_ctx = data->av_codec_ctx;
	struct user_drm_buffer *drm_buffer;
	uint32_t align_width;
	uint32_t align_height;

	switch(av_codec_ctx->pix_fmt)
	{
		case AV_PIX_FMT_YUV420P:
			align_width = SIZE_ALIGN(av_codec_ctx->width, 128);
			align_height = SIZE_ALIGN(av_codec_ctx->height, 2);
		break;
		case AV_PIX_FMT_RGB24:
			align_width = SIZE_ALIGN(av_codec_ctx->width, 64);
			align_height = SIZE_ALIGN(av_codec_ctx->height, 2);
		break;
		case AV_PIX_FMT_NV12:
			align_width = SIZE_ALIGN(av_codec_ctx->width, 64);
			align_height = SIZE_ALIGN(av_codec_ctx->height, 2);
		break;
		default:
			align_width = SIZE_ALIGN(av_codec_ctx->width, 64);
			align_height = SIZE_ALIGN(av_codec_ctx->height, 2);
		break;
	}
	log_info("ffmpeg dec video stream size:%ux%u align size:%ux%u", av_codec_ctx->width, av_codec_ctx->height, align_width, align_height);

	_reinit_shader(code_ctx);

	destory_video_texture(gl, &gl->surfaces[0]);

	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		drm_buffer = get_dma_buf(align_width, align_height, align_width, align_height, 16, true);
		if(drm_buffer == NULL)
		{
			line_error("get_dma_buf failed");
			exit(-1);
		}
		gl->surfaces[i].drm_buffer = drm_buffer;

		switch(av_codec_ctx->pix_fmt)
		{
			case AV_PIX_FMT_YUV420P:   ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
				drm_buffer->format = DRM_FORMAT_YUV420;
			break;
			case AV_PIX_FMT_RGB24:     ///< packed RGB 8:8:8, 24bpp, RGBRGB...
				drm_buffer->format = DRM_FORMAT_RGB888;
			break;
			case AV_PIX_FMT_NV12:      ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
				drm_buffer->format = DRM_FORMAT_NV12;
			break;
			default:
			break;
		}
	}
	_ffmpeg_bind_to_texture0(code_ctx);

	if(av_codec_ctx->width != align_width)
	{
		data->img_convert_ctx = sws_getContext(av_codec_ctx->width, av_codec_ctx->height, av_codec_ctx->pix_fmt, align_width, align_height, av_codec_ctx->pix_fmt, SWS_POINT, NULL, NULL, NULL); 
		data->use_sw = true;
	}

    code_ctx->has_status = true;
}

int mini_ffmpeg_rebind_buffer_to_gl(struct mini_codec* code_ctx)
{
    if(!code_ctx->has_status)
        return -1;

	return _ffmpeg_bind_to_texture0(code_ctx);
}

int mini_ffmpeg_unbind_buffer_to_gl(struct mini_codec* code_ctx)
{
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	struct display_engine *display_engine = code_ctx->display_engine;
	struct gl_obj *gl = display_engine->gl;
	struct user_drm_buffer *drm_buffer;

    if(!code_ctx->has_status)
        return -1;

	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		drm_buffer = gl->surfaces[i].drm_buffer;
        if(drm_buffer && drm_buffer->is_bind)
        {
			if(unbind_texture(gl, &gl->surfaces[i], drm_buffer) != 0)
			{
	            line_error("unbind_texture fail. idx:%d", i);
	            return -1;
			}
        }
	}
    return 0;
}

void ffmpeg_thread(void *opaque)
{
	int				i;
	int				ret;
	int				got_picture;

	struct mini_codec* code_ctx = (struct mini_codec*)opaque;
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	struct display_engine *display_engine = code_ctx->display_engine;
	struct gl_obj *gl = display_engine->gl;

	while(data->stream_idx == -1)
	{
		if(code_ctx->exit_flag)
			goto END2;
		usleep(SLEEP_DEFAULT_US);
	}

	while(av_read_frame(data->av_ctx, data->av_packet) >= 0 && !code_ctx->exit_flag)
	{
		if(data->av_packet->stream_index == data->stream_idx)
		{
			ret = avcodec_decode_video2(data->av_codec_ctx, data->av_frame, &got_picture, data->av_packet);
			if(ret < 0)
			{
				log_warning("Decode Error.");
				continue;
			}
			if(got_picture)
			{
				_on_av_frame(code_ctx, gl);
			}
		}else{
			log_warning("stream_index error", data->av_packet->stream_index);
		}
		ret = get_queue_data_len(&data->queue);
		while(ret <= F_BUFFER_SIZE)
		{
			if(data->pkt_eos_flag)
			{
				goto END1;
			}
			if(code_ctx->exit_flag)
			{
				goto END2;
			}
			usleep(SLEEP_DEFAULT_US);
			ret = get_queue_data_len(&data->queue);
		}
	}

END1:
	while (!code_ctx->exit_flag) {
		ret = avcodec_decode_video2(data->av_codec_ctx, data->av_frame, &got_picture, data->av_packet);
		if (ret != 0)
			break;
		if (!got_picture)
			break;
		_on_av_frame(code_ctx, gl);
	}
END2:
	data->frame_eos_flag = true;
	data->thread_exited = true;
	log_info("ffmpeg thread exit.");
}

int mini_ffmpeg_dec_init(struct mini_codec* code_ctx, int format)
{
    code_ctx->coder     = calloc(sizeof(struct ffmpeg_decloopdata), 1);

	struct ffmpeg_decloopdata *data = code_ctx->coder;
	unsigned char 	*out_buffer;
	int 			y_size;
	int 			ret, got_picture;
 	
    log_info("mpp init...");

	av_register_all();

	data->av_ctx = avformat_alloc_context();
	data->av_codec = avcodec_find_decoder(format);
	if(!data->av_codec)
	{
		line_error("ffmpeg not find decoder.");
		line_error("找不到解码器，请检查ffmpeg编译选项.");
		exit(-1);
	}
	av_format_set_video_codec(data->av_ctx, data->av_codec);
	data->av_ctx->video_codec_id = format;

	data->iobuffer = (unsigned char *)av_malloc(F_BUFFER_SIZE);
	AVIOContext *avio = avio_alloc_context(data->iobuffer, F_BUFFER_SIZE, 0, code_ctx, fill_iobuffer, NULL, NULL);
    if (!avio)
    {
        line_error("avio_alloc_context error!");
		line_error("不支持AVIO模式，请检查ffmpeg编译选项.");
        exit(-1);
    }
	data->av_ctx->pb = avio;
	data->av_ctx->probesize = PROBE_SIZE;
	data->av_ctx->max_analyze_duration = AV_TIME_BASE;
 
	data->av_packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(data->av_packet);
	data->av_frame = av_frame_alloc();

	data->stream_idx = -1;
	pthread_create(&data->decode_thread, NULL, ffmpeg_thread, code_ctx);
    code_ctx->vtype = CODE_FFMPEG;

    data->last_frame_idx = -1;
    data->change_gl_shader = false;
	return 0;
}

enum PUSH_STATUS mini_ffmepg_put_packet(struct mini_codec* code_ctx, char *buf, size_t size, bool pkt_eos)
{
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	int ret = write_queue_data(&data->queue, buf, size);
	data->pkt_eos_flag = pkt_eos;
	if(ret == size)
		return PUSH_OK;
	
	if(ret == QUEUE_FULL)
		return PUSH_BUFFER_FULL;

    line_error("ffmpeg: put package fail!");
	return PUSH_FAIL;
}

int ffmepg_dec_prepare_buffer(struct mini_codec* code_ctx, MppFrame frame)
{

}

int mini_ffmpeg_init_stream(struct mini_codec* code_ctx)
{

}

enum DECODE_FRAME_STATUS mini_ffmepg_get_frame(struct mini_codec* code_ctx)
{
	int ret, i, stream_tmp, frame_idx;
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	struct display_engine *display_engine = code_ctx->display_engine;
	struct gl_obj *gl = display_engine->gl;

	// printf("%lx:%s", code_ctx, __func__);
	if(data->stream_idx == -1)
	{
		ret = get_queue_data_len(&data->queue);
		if(ret <= PROBE_SIZE)
		{
			/* 通知外面推送数据 */
			log_info("ffmpeg dec probe:no data");
			return DECODE_ERROR;
		}

		if(avformat_open_input(&data->av_ctx, NULL, NULL, NULL) != 0){
			line_error("ffmpeg couldn't open input stream.");
			line_error("视频流有问题，请检查.");
			exit(-1);
		}

		if(avformat_find_stream_info(data->av_ctx, NULL) < 0){
			line_error("ffmpeg couldn't find stream information.");
			line_error("视频流有问题，请检查.");
			return DECODE_ERROR;
		}

		for(i = 0; i < data->av_ctx->nb_streams; i++)
			if(data->av_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
				stream_tmp = i;
				break;
			}
		
		if(stream_tmp == -1){
			line_error("ffmpeg Didn't find a video stream.");
			line_error("视频流有问题，请检查.");
			exit(-1);
		}

		data->av_codec_ctx = data->av_ctx->streams[stream_tmp]->codec;
		if(data->av_codec_ctx == NULL){
			line_error("av_codec_ctx not found.");
			line_error("视频流有问题，请检查.");
			exit(-1);
		}

		// data->av_codec_ctx = avcodec_alloc_context3(data->av_codec);
		// avcodec_parameters_to_context(data->av_codec_ctx, data->av_ctx->streams[stream_tmp]->codecpar);
		data->av_codec_ctx->thread_count = 4;
		data->av_codec_ctx->thread_type = FF_THREAD_FRAME;

		if(avcodec_open2(data->av_codec_ctx, data->av_codec, NULL) < 0){
			line_error("Could not open codec.");
			line_error("ffmpeg出错.");
			exit(-1);
		}
		_ffmpeg_alloc_buffer(code_ctx);
		data->stream_idx = stream_tmp;
	}

	if(data->stream_idx == -1)
	{
		line_error("ffmpeg dec error, stream_idx is -1.");
		return DECODE_ERROR;
	}

	if(data->show_frame == data->dec_frame && data->frame_eos_flag)
		return DECODE_EOS;

    atomic_store(&code_ctx->status, DECODEING);
	frame_idx = data->show_frame % FRAME_COUNT;
	// for (i = 0; i < 5; ++i)
	// {
	// 	/* 等待帧被解码，不能等待太久 */
	// 	if(atomic_load(&data->frame_status[frame_idx]))
	// 	{
	// 		break;
	// 	}else{
	// 		ret = get_queue_data_len(&data->queue);
	// 		if(ret <= F_BUFFER_SIZE)
	// 		{
	// 			/* 通知外面推送数据 */
 //        		atomic_store(&code_ctx->status, CODER_STOP);
	// 			return DECODE_LACK_DATA;
	// 		}
	// 		usleep(SLEEP_DEFAULT_US);
	// 	}
	// }
	if(atomic_load(&data->frame_status[frame_idx]))
	{
		gl->surface_idx = frame_idx;
		data->show_frame++;
		if(data->last_frame_idx != -1)
		{
			atomic_store(&data->frame_status[data->last_frame_idx], 0);
		}
		data->last_frame_idx = frame_idx;
		return DECODE_OK;
	}
	atomic_store(&code_ctx->status, CODER_STOP);

	return DECODE_TIMEOUT;
}

int mini_ffmepg_dec_release(struct mini_codec* code_ctx)
{
	struct ffmpeg_decloopdata *data = code_ctx->coder;
	struct display_engine *display_engine = code_ctx->display_engine;
	struct gl_obj *gl = display_engine->gl;
	uint32_t counter = 0;

	while(!data->thread_exited)
	{
		counter++;
		if(counter > 50)
		{
			usleep(SLEEP_DEFAULT_US);
			log_info("ffmepg dec: waiting for thread exit.");
			counter = 0;
		}
	}

	if(data->use_sw)
	{
		sws_freeContext(data->img_convert_ctx);
	}

	if(data->stream_idx != -1)
		avcodec_close(data->av_codec_ctx);

	avformat_free_context(data->av_ctx);

	av_free_packet(data->av_packet);

	for (int i = 0; i < FRAME_COUNT; ++i)
	{
		destory_video_texture(gl, &gl->surfaces[i]);
	}

	free(data);
	log_info("ffmpeg dec release.");
}