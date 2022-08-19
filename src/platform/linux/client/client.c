#include <stdlib.h>

#include "util.h"
#include "string.h"
#include "buffer.h"
#include "module.h"
#include "fb_in.h"
#include "fb_out.h"
#include "encodec.h"

struct module_data * in_dev;
struct module_data * out_dev;
struct module_data * enc_dev;
// struct module_data * dec_dev;

FILE *fp;
int on_event()
{
    struct common_buffer *pkt;
    struct common_buffer * buffer;

    buffer = fb_in_get_fb(in_dev);
    fb_out_put_fb(out_dev, buffer);
    encodec_push_fb(enc_dev, buffer);
    pkt = encodec_get_package(enc_dev);
    fwrite(pkt->ptr, pkt->size, 1, fp);
    return 0;
}

int main(int argc, char* argv[])
{
    struct encodec_info enc_info;
    struct frame_buffer_info fb_info;

    in_dev = fb_in_init_dev();
    fb_in_get_info(in_dev, &fb_info);

    out_dev = fb_out_init_dev();
    fb_out_set_info(out_dev, fb_info);

    memcpy(&enc_info.fb_info, &fb_info, sizeof(fb_info));
    enc_info.stream_fmt = STREAM_H265;
    enc_info.quality = 80;
    enc_dev = encodec_init_dev(enc_info);

    fp = fopen("out.h264", "wb+");
    if(enc_dev == NULL)
        exit(-1);

    in_dev->on_event = on_event;
    out_dev->on_event = on_event;
    // fb_in_main_loop(in_dev);
    fb_out_main_loop(out_dev);
    return 0;
}