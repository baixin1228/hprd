#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "util.h"
#include "string.h"
#include "buffer.h"
#include "module.h"
#include "fb_in.h"
#include "fb_out.h"
#include "encodec.h"
#include "decodec.h"

struct module_data * in_dev;
struct module_data * out_dev;
struct module_data * enc_dev;
struct module_data * dec_dev;

struct common_buffer *pkt = NULL;
struct common_buffer * buffer = NULL;
pthread_t encode_thread_info;
void *encode_thread(void *op)
{
    while(1)
    {
        buffer = fb_in_get_fb(in_dev);
        encodec_push_fb(enc_dev, buffer);
        pkt = encodec_get_package(enc_dev);
        decodec_push_pkt(dec_dev, pkt);
        usleep(30000);
    }
    return NULL;
}

int on_event()
{
    struct common_buffer * _buffer;

    if(pkt != NULL)
    {
        _buffer = decodec_get_fb(dec_dev);
        if(_buffer != NULL)
        {
            fb_out_put_fb(out_dev, _buffer);
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    struct encodec_info enc_info;
    struct frame_buffer_info fb_info;
    struct decodec_info dec_info;

    in_dev = fb_in_init_dev();
    fb_in_get_info(in_dev, &fb_info);

    memcpy(&enc_info.fb_info, &fb_info, sizeof(fb_info));
    enc_info.stream_fmt = STREAM_H265;
    enc_info.quality = 80;
    enc_dev = encodec_init_dev(enc_info);

    dec_info.stream_fmt = enc_info.stream_fmt;
    dec_dev = decodec_init_dev(dec_info);

    fb_info.format = YUV420P;
    out_dev = fb_out_init_dev();
    fb_out_set_info(out_dev, fb_info);

    in_dev->on_event = on_event;
    out_dev->on_event = on_event;
    
    pthread_create(&encode_thread_info, NULL, encode_thread, NULL);
    fb_out_main_loop(out_dev);
    return 0;
}