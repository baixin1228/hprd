#include <x265.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "encodec.h"

#define PKT_BUFFER_SIZE (1 * 1024 * 1024)

struct x265_enc_data{
	x265_param *x265_params;
	x265_encoder *x265_enc_ctx;
	x265_picture *x265_pic_in;

	int plan1_size;
	x265_nal *pp_nal;
	uint32_t pi_nal;
    int enc_status;

    struct common_buffer ret_pkt;
};

static int _com_fb_fmt_to_x265_fmt(enum COMMON_BUFFER_FORMAT format)
{
    switch(format)
    {
        case ARGB8888:
            return X265_CSP_RGB;
        break;
        case YUV420P:
            return X265_CSP_I420;
        break;
        case NV12:
            return X265_CSP_NV12;
        break;
        default:
            return X265_CSP_I420;
        break;
    }
}

static int x265_enc_init(struct module_data *encodec_dev, struct encodec_info enc_info)
{
    struct x265_enc_data *enc_data;

    enc_data = calloc(1, sizeof(*enc_data));
    if(!enc_data)
    {
        func_error("calloc fail, check free memery.");
        goto FAIL1;
    }
 
	int csp = _com_fb_fmt_to_x265_fmt(enc_info.fb_info.format);
	csp = X265_CSP_I420;
	int width = enc_info.fb_info.width;
	int height = enc_info.fb_info.height;
 
	enc_data->x265_params = x265_param_alloc();
	if(enc_data->x265_params == NULL)
	{
		func_error("x265_param_alloc fail.");
		goto FAIL2;
	}

	/* rt enc */
	x265_param_default_preset(enc_data->x265_params, "ultrafast", "zerolatency");
	/* write sps,pps before keyframe */
	enc_data->x265_params->bRepeatHeaders = 1;
	enc_data->x265_params->internalCsp = csp;
	enc_data->x265_params->sourceWidth = width;
	enc_data->x265_params->sourceHeight = height;
	enc_data->x265_params->fpsNum = 60;
	enc_data->x265_params->fpsDenom = 1;

	enc_data->x265_params->rc.rateControlMode = X265_RC_CQP;
	enc_data->x265_params->rc.qp = 30;

	enc_data->x265_enc_ctx = x265_encoder_open(enc_data->x265_params);
	if(enc_data->x265_enc_ctx == NULL){
		func_error("x265_encoder_open err.");
		goto FAIL3;
	}

	enc_data->plan1_size = width * height;

	enc_data->x265_pic_in = x265_picture_alloc();
	x265_picture_init(enc_data->x265_params, enc_data->x265_pic_in);
	switch(csp){
		case X265_CSP_RGB:
		{
			enc_data->x265_pic_in->stride[0] = width * 3;
			enc_data->x265_pic_in->stride[1] = 0;
			enc_data->x265_pic_in->stride[2] = 0;
			break;
		}
		case X265_CSP_I420:
		{
			enc_data->x265_pic_in->stride[0] = width;
			enc_data->x265_pic_in->stride[1] = width / 2;
			enc_data->x265_pic_in->stride[2] = width / 2;
			break;
		}
		case X265_CSP_NV12:
		{
			enc_data->x265_pic_in->stride[0] = width;
			enc_data->x265_pic_in->stride[1] = width;
			enc_data->x265_pic_in->stride[2] = 0;
			break;
		}
		default:
		{
			func_error("Colorspace Not Support.");
			goto FAIL4;
		}
	}
	
	enc_data->ret_pkt.ptr = malloc(PKT_BUFFER_SIZE);
	encodec_dev->priv = enc_data;
    return 0;
FAIL4:
	x265_encoder_close(enc_data->x265_enc_ctx);
    x265_picture_free(enc_data->x265_pic_in);
FAIL3:
    x265_param_free(enc_data->x265_params);
FAIL2:
    free(enc_data);
FAIL1:
    return -1;
}

static int x265_frame_enc(struct module_data *encodec_dev, struct common_buffer *buffer)
{
    struct x265_enc_data *enc_data = encodec_dev->priv;

    if(buffer)
    {
		switch(enc_data->x265_params->internalCsp){
			case X265_CSP_RGB:
			{
				enc_data->x265_pic_in->planes[0] = buffer->ptr;
				enc_data->x265_pic_in->planes[1] = NULL;
				enc_data->x265_pic_in->planes[2] = NULL;
				break;
			}
			case X265_CSP_I420:
			{
				enc_data->x265_pic_in->planes[0] = buffer->ptr;
				enc_data->x265_pic_in->planes[1] = buffer->ptr + enc_data->plan1_size;
				enc_data->x265_pic_in->planes[2] = buffer->ptr + enc_data->plan1_size * 5 / 4;
				break;
			}
			case X265_CSP_NV12:
			{
				enc_data->x265_pic_in->planes[0] = buffer->ptr;
				enc_data->x265_pic_in->planes[1] = buffer->ptr + enc_data->plan1_size;
				enc_data->x265_pic_in->planes[2] = NULL;
				break;
			}
			default:
			{
				func_error("Colorspace Not Support.");
				return -1;
			}
		}

		enc_data->enc_status = x265_encoder_encode(enc_data->x265_enc_ctx,
			&enc_data->pp_nal,
			&enc_data->pi_nal,
			enc_data->x265_pic_in,
			NULL);
    }else{
		enc_data->enc_status = x265_encoder_encode(enc_data->x265_enc_ctx,
			&enc_data->pp_nal,
			&enc_data->pi_nal,
			NULL,
			NULL);
    }


	if(enc_data->enc_status == 1)
	{
		return 0;
	}else{
		return -1;
	}
}

static  struct common_buffer *x265_enc_get_pkt(struct module_data *encodec_dev)
{
	int i;
    struct x265_enc_data *enc_data = encodec_dev->priv;

	if(enc_data->enc_status)
	{
		enc_data->ret_pkt.size = 0;
		for(i=0; i < enc_data->pi_nal; i++)
		{
			memcpy(enc_data->ret_pkt.ptr, enc_data->pp_nal[i].payload, enc_data->pp_nal[i].sizeBytes);
			enc_data->ret_pkt.size += enc_data->pp_nal[i].sizeBytes;
		}
		log_info("x265 enc success, pkt size:%dkb.", enc_data->ret_pkt.size / 1024);
		return &enc_data->ret_pkt;
	}

	log_info("x265 enc no data, pi_nal:%d.", enc_data->pi_nal);
	return NULL;
}

static int x265_enc_release(struct module_data *encodec_dev)
{
    struct x265_enc_data *enc_data = encodec_dev->priv;

    free(enc_data->ret_pkt.ptr);
	x265_encoder_close(enc_data->x265_enc_ctx);
    x265_picture_free(enc_data->x265_pic_in);
    x265_param_free(enc_data->x265_params);
    free(enc_data);
    return 0;
}

struct encodec_ops x265_encodec_dev = 
{
    .name               = "x265_encodec_dev",
    .init               = x265_enc_init,
    .push_fb            = x265_frame_enc,
    .get_package        = x265_enc_get_pkt,
    .release            = x265_enc_release
};

REGISTE_ENCODEC_DEV(x265_encodec_dev, DEVICE_PRIO_LOW);