#include <stdlib.h>

#include "util.h"
#include "string.h"
#include "buffer.h"
#include "module.h"
#include "fb_in/fb_in.h"
#include "fb_out/fb_out.h"
#include "codec/encodec.h"

struct module_data * in_dev;
struct module_data * out_dev;
struct module_data * enc_dev;

int on_event()
{
    struct common_buffer * buffer = fb_in_get_fb(in_dev);
    fb_out_put_fb(out_dev, buffer);
    encodec_push_fb(enc_dev, buffer);
    encodec_get_package(enc_dev);
    // log_info("on_event");
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

    if(enc_dev == NULL)
        exit(-1);
    // fb_out_set_info(enc_dev, fb_info);

    in_dev->on_event = on_event;
    out_dev->on_event = on_event;
    // fb_in_main_loop(in_dev);
    fb_out_main_loop(out_dev);
    return 0;
}