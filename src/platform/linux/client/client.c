#include <stdlib.h>

#include "util.h"
#include "buffer.h"
#include "module.h"
#include "fb_in/fb_in.h"
#include "fb_out/fb_out.h"

struct module_data * in_dev;
struct module_data * out_dev;
int on_event()
{
    struct common_buffer * buffer = fb_in_get_fb(in_dev);
    fb_out_put_fb(out_dev, buffer);
    log_info("on_event");
    return 0;
}

int main(int argc, char* argv[])
{
    struct frame_buffer_info fb_info;

    in_dev = fb_in_init_dev();
    fb_in_get_info(in_dev, &fb_info);

    out_dev = fb_out_init_dev();
    fb_out_set_info(out_dev, fb_info);

    in_dev->on_event = on_event;
    out_dev->on_event = on_event;
    // fb_in_main_loop(in_dev);
    fb_out_main_loop(out_dev);
    return 0;
}