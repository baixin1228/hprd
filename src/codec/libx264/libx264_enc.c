#include <stdint.h>
#include <x264.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "encodec.h"
#include "fb_convert.h"

#define PKT_BUFFER_SIZE (10 * 1024 * 1024)

struct x264_enc_data{
	x264_param_t x264_params;
	x264_t *x264_enc_ctx;

	x264_picture_t *x264_pic_in;
	x264_picture_t *x264_pic_out;

	int plan1_size;
	x264_nal_t *pp_nals;
	int i_nal;
    int enc_status;

    uint64_t success_fb_cnt;

    struct common_buffer ret_pkt;

    struct common_buffer fc_buf;
    struct module_data *fc;
    bool use_fc;
};

static int _com_fb_fmt_to_x264_fmt(enum COMMON_BUFFER_FORMAT format)
{
    switch(format)
    {
        case ARGB8888:
        {
        	func_error("not support fb format ARGB8888.");
            return -1;
        	break;
        }
        case YUV420P:
            return X264_CSP_I420;
        break;
        case NV12:
            return X264_CSP_NV12;
        break;
        default:
            return X264_CSP_I420;
        break;
    }
}

static int x264_enc_init(struct module_data *encodec_dev, struct encodec_info enc_info)
{
    struct x264_enc_data *enc_data;

    enc_data = calloc(1, sizeof(*enc_data));
    if(!enc_data)
    {
        func_error("calloc fail, check free memery.");
        goto FAIL1;
    }
 
 	int width = enc_info.fb_info.width;
	int height = enc_info.fb_info.height;

	int csp = _com_fb_fmt_to_x264_fmt(enc_info.fb_info.format);
	if(csp == -1)
	{
		enc_data->fc = fc_init_dev();
		enc_data->fc_buf.width = width;
		enc_data->fc_buf.height = height;
		enc_data->fc_buf.hor_stride = width;
		enc_data->fc_buf.ver_stride = height;
		enc_data->fc_buf.format = YUV420P;
		enc_data->fc_buf.bpp = 16;
		enc_data->fc_buf.ptr = malloc(width * height * 2);
		csp = X264_CSP_I420;
		enc_data->use_fc = true;
	}

	/* rt enc */
	x264_param_default_preset(&enc_data->x264_params, "ultrafast", "zerolatency");
	enc_data->x264_params.i_threads  = X264_SYNC_LOOKAHEAD_AUTO;
	/* write sps,pps before keyframe */
	enc_data->x264_params.b_repeat_headers = 1;
	enc_data->x264_params.i_csp = csp;
	enc_data->x264_params.i_width = width;
	enc_data->x264_params.i_height = height;
	enc_data->x264_params.i_fps_num = 30;
	enc_data->x264_params.i_fps_den = 1;
	enc_data->x264_params.i_keyint_min = 2;
	enc_data->x264_params.i_keyint_max = 20;

	enc_data->x264_params.i_threads = 4;

	enc_data->x264_params.rc.i_rc_method = X264_RC_CQP;
	enc_data->x264_params.rc.i_qp_constant = 20;

	enc_data->x264_enc_ctx = x264_encoder_open(&enc_data->x264_params);
	if(enc_data->x264_enc_ctx == NULL){
		func_error("x264_encoder_open err.");
		goto FAIL2;
	}

	enc_data->plan1_size = width * height;

	enc_data->x264_pic_in = (x264_picture_t *)calloc(1, sizeof(x264_picture_t));
	enc_data->x264_pic_out = (x264_picture_t *)calloc(1, sizeof(x264_picture_t));
	x264_picture_init(enc_data->x264_pic_out);
	x264_picture_alloc(enc_data->x264_pic_in, csp, width, height);
	switch(csp){
		case X264_CSP_RGB:
		{
			enc_data->x264_pic_in->img.i_stride[0] = width * 3;
			enc_data->x264_pic_in->img.i_stride[1] = 0;
			enc_data->x264_pic_in->img.i_stride[2] = 0;
			break;
		}
		case X264_CSP_I420:
		{
			enc_data->x264_pic_in->img.i_stride[0] = width;
			enc_data->x264_pic_in->img.i_stride[1] = width / 2;
			enc_data->x264_pic_in->img.i_stride[2] = width / 2;
			break;
		}
		case X264_CSP_NV12:
		{
			enc_data->x264_pic_in->img.i_stride[0] = width;
			enc_data->x264_pic_in->img.i_stride[1] = width;
			enc_data->x264_pic_in->img.i_stride[2] = 0;
			break;
		}
		default:
		{
			func_error("Colorspace Not Support.");
			goto FAIL3;
		}
	}
	
	enc_data->ret_pkt.ptr = malloc(PKT_BUFFER_SIZE);
	encodec_dev->priv = enc_data;
    return 0;
FAIL3:
	x264_encoder_close(enc_data->x264_enc_ctx);
    x264_picture_clean(enc_data->x264_pic_in);
    free(enc_data->x264_pic_in);
    free(enc_data->x264_pic_out);
FAIL2:
    free(enc_data);
FAIL1:
    return -1;
}

static int x264_frame_enc(struct module_data *encodec_dev, struct common_buffer *buffer)
{
    struct x264_enc_data *enc_data = encodec_dev->priv;
    struct common_buffer *_buffer;

    if(enc_data->use_fc)
    {
    	fc_convert(enc_data->fc, buffer, &enc_data->fc_buf);
    	_buffer = &enc_data->fc_buf;
    }else{
    	_buffer = buffer;
    }

    if(_buffer)
    {
		switch(enc_data->x264_params.i_csp){
			case X264_CSP_RGB:
			{
				enc_data->x264_pic_in->img.plane[0] = (uint8_t *)(_buffer->ptr);
				enc_data->x264_pic_in->img.plane[1] = NULL;
				enc_data->x264_pic_in->img.plane[2] = NULL;
				break;
			}
			case X264_CSP_I420:
			{
				enc_data->x264_pic_in->img.plane[0] = (uint8_t *)(_buffer->ptr);
				enc_data->x264_pic_in->img.plane[1] = (uint8_t *)(_buffer->ptr + enc_data->plan1_size);
				enc_data->x264_pic_in->img.plane[2] = (uint8_t *)(_buffer->ptr + enc_data->plan1_size * 5 / 4);
				break;
			}
			case X264_CSP_NV12:
			{
				enc_data->x264_pic_in->img.plane[0] = (uint8_t *)(_buffer->ptr);
				enc_data->x264_pic_in->img.plane[1] = (uint8_t *)(_buffer->ptr + enc_data->plan1_size);
				enc_data->x264_pic_in->img.plane[2] = NULL;
				break;
			}
			default:
			{
				func_error("Colorspace Not Support.");
				return -1;
			}
		}
		enc_data->enc_status = x264_encoder_encode(enc_data->x264_enc_ctx,
			&enc_data->pp_nals,
			&enc_data->i_nal,
			enc_data->x264_pic_in,
			enc_data->x264_pic_out);
    }else{
		enc_data->enc_status = x264_encoder_encode(enc_data->x264_enc_ctx,
			&enc_data->pp_nals,
			&enc_data->i_nal,
			NULL,
			enc_data->x264_pic_out);
    }

    if(enc_data->x264_pic_out->i_type == X264_TYPE_IDR)
    {
    	log_info("x264 find IDR");
    }
    if(enc_data->x264_pic_out->i_type == X264_TYPE_I)
    {
    	log_info("x264 find I");
    }
    // if(enc_data->x264_pic_out->i_type == X264_TYPE_P)
    // {
    // 	log_info("x264 find P");
    // }
    // if(enc_data->x264_pic_out->i_type == X264_TYPE_B)
    // {
    // 	log_info("x264 find B");
    // }

	if(enc_data->enc_status != 0)
	{
	    enc_data->success_fb_cnt++;
    	// printf("i_frame_size:%6d record:%lu enc:%lu\n", enc_data->enc_status, _buffer->id, enc_data->success_fb_cnt);
		return 0;
	}else{
		return -1;
	}
}

static  struct common_buffer *x264_enc_get_pkt(struct module_data *encodec_dev)
{
	int i;
    struct x264_enc_data *enc_data = encodec_dev->priv;

	if(enc_data->enc_status)
	{
		enc_data->ret_pkt.size = 0;
		for(i=0; i < enc_data->i_nal; i++)
		{
			// if(enc_data->pp_nals[i].i_type == NAL_PPS)
			// {
			// 	log_info("x264 find PPS.");
			// }
			// if(enc_data->pp_nals[i].i_type == NAL_SPS)
			// {
			// 	log_info("x264 find SPS.");
			// }
			memcpy(enc_data->ret_pkt.ptr + enc_data->ret_pkt.size, enc_data->pp_nals[i].p_payload, enc_data->pp_nals[i].i_payload);
			enc_data->ret_pkt.size += enc_data->pp_nals[i].i_payload;
		}
		return &enc_data->ret_pkt;
	}

	return NULL;
}

static int x264_enc_release(struct module_data *encodec_dev)
{
    struct x264_enc_data *enc_data = encodec_dev->priv;

    free(enc_data->ret_pkt.ptr);
	x264_encoder_close(enc_data->x264_enc_ctx);
    x264_picture_clean(enc_data->x264_pic_in);
    free(enc_data->x264_pic_in);
    free(enc_data->x264_pic_out);
    free(enc_data);
    return 0;
}

struct encodec_ops x264_encodec_dev = 
{
    .name               = "x264_encodec_dev",
    .init               = x264_enc_init,
    .push_fb            = x264_frame_enc,
    .get_package        = x264_enc_get_pkt,
    .release            = x264_enc_release
};

REGISTE_ENCODEC_DEV(x264_encodec_dev, DEVICE_PRIO_MIDDLE);