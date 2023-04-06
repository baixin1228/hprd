#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "util.h"
#include "decodec.h"
#include "ffmpeg_util.h"
#include "mini_ring_queue.h"

#define SLEEP_DEFAULT_US 10000
#define F_BUFFER_SIZE 8196
#define CACHE_FB_COUNT 2

struct ffmpeg_dec_data{
    AVFormatContext *av_ctx;
    AVCodecContext  *av_codec_ctx;
    AVCodec         *av_codec;

    /* capture and display */
    AVPacket        *av_packet;
    AVFrame         *av_frame;

    unsigned char * iobuffer;
    
    bool pkt_eos_flag;
    bool frame_eos_flag;

    bool use_sw;
    struct SwsContext *img_convert_ctx;

    int stream_idx;
    struct mini_data_queue *queue;
    uint64_t show_frame_idx;
    uint64_t dec_frame_idx;

    pthread_t decode_thread;
    bool is_thread_exit;
    bool exit_flag;
    bool thread_exited;

    uint64_t put_count;

    struct common_buffer fbs[CACHE_FB_COUNT];
};

int fill_iobuffer(void * opaque, uint8_t *buf, int bufsize){
	struct ffmpeg_dec_data *data = (struct ffmpeg_dec_data*)opaque;
	int ret;
	uint32_t counter = 0;

	while(!data->pkt_eos_flag && !data->exit_flag)
	{
		ret = get_queue_data_count(data->queue);
		if(ret <= bufsize)
		{
			counter++;
			usleep(SLEEP_DEFAULT_US);
			if(counter > 100)
			{
				log_info("fill_iobuffer:waiting for data, len:%d.\n", bufsize);
				counter = 0;
			}
		}else{
			break;
		}
	}

	if(!data->pkt_eos_flag && !data->exit_flag)
	{
		ret = pop_queue_data(data->queue, buf, bufsize);
		return ret;
	}

	return 0;
}

int _on_av_frame(struct ffmpeg_dec_data *data)
{
	uint32_t print_counter 			= 0;
	AVFrame *frame 					= data->av_frame;

	uint32_t dec_frame_idx 			= data->dec_frame_idx % CACHE_FB_COUNT;

	while(data->dec_frame_idx >= data->show_frame_idx + CACHE_FB_COUNT)
	{
		if(data->exit_flag || data->pkt_eos_flag)
		{
			return 0;
		}
		if(print_counter > 100)
		{
			log_warning("waiting for frame randered, dec_idx:%d show_idx:%d", data->dec_frame_idx, data->show_frame_idx);
			print_counter = 0;
		}
		print_counter++;
		usleep(SLEEP_DEFAULT_US);
	}

	struct common_buffer *buffer = &data->fbs[dec_frame_idx];
	buffer->id = frame->pts;
	switch(frame->format)
	{
		case AV_PIX_FMT_YUV420P:
		{
			smp_frame_copy(buffer->ptr, 
			frame->width,
			frame->data[0],
			frame->linesize[0],
			frame->width,
			buffer->height);
			smp_frame_copy(buffer->ptr + frame->width * buffer->height,
				frame->width / 2,
				frame->data[1],
				frame->linesize[1],
				frame->width / 2,
				buffer->height / 2);
			smp_frame_copy(buffer->ptr + frame->width * buffer->height * 5 / 4,
				frame->width / 2,
				frame->data[2],
				frame->linesize[2],
				frame->width / 2,
				buffer->height / 2);
			break;
		}
		case AV_PIX_FMT_RGB24:     ///< packed RGB 8:8:8, 24bpp, RGBRGB...
		{
			smp_frame_copy(buffer->ptr,
			frame->linesize[0],
			frame->data[0],
			frame->linesize[0],
			frame->linesize[0],
			buffer->height * 3);
			break;
		}
		case AV_PIX_FMT_NV12:      ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
		{
			smp_frame_copy(buffer->ptr,
			frame->linesize[0],
			frame->data[0],
			frame->linesize[0],
			frame->linesize[0],
			buffer->height);
			smp_frame_copy(buffer->ptr + frame->linesize[0] * buffer->height,
					frame->linesize[0],
					frame->data[0],
					frame->linesize[0],
					frame->linesize[0],
					buffer->height);
			break;
		}
	}
	atomic_fetch_add(&data->dec_frame_idx, 1);
	return 0;
}

int _ffmpeg_alloc_buffer(struct ffmpeg_dec_data *data)
{
	AVCodecContext  *av_codec_ctx 	= data->av_codec_ctx;

	if(av_codec_ctx->width == 0 || av_codec_ctx->height == 0)
	{
		log_error("stream width or height is 0.\n");
		return -1;
	}

	for (int i = 0; i < CACHE_FB_COUNT; ++i)
	{
		data->fbs[i].width = av_codec_ctx->width;
		data->fbs[i].hor_stride = av_codec_ctx->width;
		data->fbs[i].height = av_codec_ctx->height;
		data->fbs[i].ver_stride = av_codec_ctx->height;

		switch(av_codec_ctx->pix_fmt)
		{
			case AV_PIX_FMT_YUV420P:
				data->fbs[i].format = YUV420P;
				data->fbs[i].bpp = 16;
				data->fbs[i].ptr = malloc(av_codec_ctx->width * av_codec_ctx->height * 2);
			break;
			case AV_PIX_FMT_RGB32:
				data->fbs[i].format = ARGB8888;
				data->fbs[i].bpp = 32;
				data->fbs[i].ptr = malloc(av_codec_ctx->width * av_codec_ctx->height * 4);
			break;
			case AV_PIX_FMT_NV12:
				data->fbs[i].format = NV12;
				data->fbs[i].bpp = 16;
				data->fbs[i].ptr = malloc(av_codec_ctx->width * av_codec_ctx->height * 2);
			break;
			default:
				log_error("frame format not support.");
				return -1;
			break;
		}
	}
	return 0;
}

void *_ffmpeg_dec_thread(void *opaque)
{
	uint32_t		print_counter = 0;
	int				ret;

	struct ffmpeg_dec_data *data = (struct ffmpeg_dec_data*)opaque;

	while(data->stream_idx == -1)
	{
		if(data->exit_flag)
			goto END1;

		if(print_counter > 100)
		{
			log_warning("waiting for stream_idx");
			print_counter = 0;
		}
		print_counter++;
		usleep(SLEEP_DEFAULT_US);
	}

	while(!data->exit_flag && av_read_frame(data->av_ctx, data->av_packet) >= 0)
	{
		if(data->av_packet->stream_index == data->stream_idx)
		{
			ret = avcodec_send_packet(data->av_codec_ctx, data->av_packet);
			switch(ret)
			{
				case 0:
				{
					ret = avcodec_receive_frame(data->av_codec_ctx, data->av_frame);
					if(ret == 0)
					{
						_on_av_frame(data);
					}
					break;
				}
				case AVERROR(EAGAIN):
				{
					break;
				}
				case AVERROR_EOF:
				{
					goto END2;
					break;
				}
				case AVERROR(EINVAL):
				{
					log_error("codec not open.");
					break;
				}
				case AVERROR_capture_CHANGED:
				{
					break;
				}
			}
		}else{
			log_warning("stream_index error:%d", data->av_packet->stream_index);
		}
	}

END2:
	while (!data->exit_flag) {
		ret = avcodec_receive_frame(data->av_codec_ctx, data->av_frame);
		if(ret == 0)
		{
			_on_av_frame(data);
		}else{
			break;
		}
	}
END1:
	data->frame_eos_flag = true;
	data->thread_exited = true;
	log_info("ffmpeg dec thread exit.");

	return NULL;
}

int ffmpeg_dec_init(struct module_data *dev, struct decodec_info enc_info)
{
    struct ffmpeg_dec_data 	*data;
    int 					format;

    if(enc_info.stream_fmt == 0)
        goto FAIL1;

    format = com_fmt_to_av_fmt(enc_info.stream_fmt);

    data = calloc(1, sizeof(*data));
    if(!data)
    {
        log_error("calloc fail, check free memery.");
        goto FAIL1;
    }

    data->queue = calloc(1, sizeof(*data->queue));
    if(!data->queue)
    {
        log_error("calloc fail, check free memery.");
        goto FAIL2;
    }

	data->av_ctx = avformat_alloc_context();
	data->av_codec = avcodec_find_decoder(format);
	if(!data->av_codec)
	{
		log_error("ffmpeg dec not find decoder:%x.", format);
        goto FAIL3;
	}
	data->av_ctx->video_codec_id = format;

	data->iobuffer = (unsigned char *)av_malloc(F_BUFFER_SIZE);
	AVIOContext *avio = avio_alloc_context(data->iobuffer, F_BUFFER_SIZE, 0, data, fill_iobuffer, NULL, NULL);
    if (!avio)
    {
        log_error("avio_alloc_context error!");
        goto FAIL4;
    }
	data->av_ctx->pb = avio;
 
	data->av_packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(data->av_packet);
	data->av_frame = av_frame_alloc();

	data->stream_idx = -1;
	dev->priv = data;

	return 0;

FAIL4:
	av_free(data->iobuffer);
	avformat_free_context(data->av_ctx);
FAIL3:
    free(data->queue);
FAIL2:
    free(data);
FAIL1:
    return -1;
}

static enum PUSH_STATUS ffmepg_put_packet(struct module_data *dev, struct common_buffer *buffer)
{
	struct ffmpeg_dec_data *data = dev->priv;
	int ret;
	if(buffer->ptr){
		ret = queue_append_data(data->queue, buffer->ptr, buffer->size);
	}else{
		data->pkt_eos_flag = true;
		data->put_count++;
		return PUSH_OK;
	}

	if(ret == buffer->size)
	{
		data->put_count++;
		return PUSH_OK;
	}
	
	if(ret == QUEUE_FULL)
		return PUSH_BUFFER_FULL;

    log_error("put package fail!");
	return PUSH_FAIL;
}

static struct common_buffer *ffmepg_get_frame(struct module_data *dev)
{
	int i, stream_tmp, frame_idx, result;
	AVCodecParameters *origin_par;
	struct ffmpeg_dec_data *data = dev->priv;

	printf("put:%lu dec:%lu show:%lu\n", data->put_count, data->dec_frame_idx, data->show_frame_idx);

	if(data->stream_idx == -1)
	{
		if(avformat_open_capture(&data->av_ctx, NULL, NULL, NULL) != 0){
			log_error("ffmpeg couldn't open capture stream.");
			return NULL;
		}

		if(avformat_find_stream_info(data->av_ctx, NULL) < 0){
			log_error("ffmpeg couldn't find stream information.");
			return NULL;
		}

		for(i = 0; i < data->av_ctx->nb_streams; i++)
			if(data->av_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
				stream_tmp = i;
				break;
			}
		
		if(stream_tmp == -1){
			log_error("ffmpeg Didn't find a video stream.");
			return NULL;
		}

	    origin_par = data->av_ctx->streams[stream_tmp]->codecpar;

	    data->av_codec = avcodec_find_decoder(origin_par->codec_id);
	    if (!data->av_codec) {
	        log_error("Can't find decoder");
			return NULL;
	    }

	    data->av_codec_ctx = avcodec_alloc_context3(data->av_codec);
	    if (!data->av_codec_ctx) {
	        log_error("Can't allocate decoder context");
			return NULL;
	    }

	    result = avcodec_parameters_to_context(data->av_codec_ctx, origin_par);
	    if (result) {
	        log_error("Can't copy decoder context");
			return NULL;
	    }


		data->av_codec_ctx->thread_count = 1;
		data->av_codec_ctx->thread_type = FF_THREAD_FRAME;

		if(avcodec_open2(data->av_codec_ctx, data->av_codec, NULL) < 0){
			log_error("Could not open codec.");
			return NULL;
		}
		if(_ffmpeg_alloc_buffer(data) == 0)
		{
			data->stream_idx = stream_tmp;
			pthread_create(&data->decode_thread, NULL, _ffmpeg_dec_thread, data);
		}
	}

	if(data->stream_idx == -1)
	{
		log_error("ffmpeg dec error, stream_idx is -1.");
		return NULL;
	}

	if(data->show_frame_idx == data->dec_frame_idx && data->frame_eos_flag)
		return NULL;

	if(data->dec_frame_idx > data->show_frame_idx)
	{
		frame_idx = data->show_frame_idx % CACHE_FB_COUNT;
		atomic_fetch_add(&data->show_frame_idx, 1);
		return &data->fbs[frame_idx];
	}

	return NULL;
}

static int ffmepg_dec_release(struct module_data *dev)
{
	struct ffmpeg_dec_data *data = dev->priv;
	uint32_t print_counter = 0;

	while(!data->thread_exited)
	{
		print_counter++;
		if(print_counter > 100)
		{
			log_info("ffmepg dec: waiting for thread exit.");
			print_counter = 0;
		}
		usleep(SLEEP_DEFAULT_US);
	}

	if(data->stream_idx != -1)
		avcodec_close(data->av_codec_ctx);

	avformat_free_context(data->av_ctx);

	av_packet_free(&data->av_packet);

	for (int i = 0; i < CACHE_FB_COUNT; ++i)
	{
		free(data->fbs[i].ptr);
	}

	if(data->queue)
	{
	    free(data->queue);
	    data->queue = NULL;
	}
	
	free(data);
	log_info("ffmpeg dec release.");
	return 0;
}

struct decodec_ops dev_ops = 
{
    .name               = "ffmpeg_decodec",
    .init               = ffmpeg_dec_init,
    .put_pkt            = ffmepg_put_packet,
    .get_info 			= ,
    .map_buffer 		= ,
    .get_buffer 		= ffmepg_get_frame,
    .put_buffer	        = ,
    .unmap_buffer 		= ,
    .release            = ffmepg_dec_release
};