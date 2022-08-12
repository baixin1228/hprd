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
    struct common_buffer * buffer = get_frame_buffer(in_dev);
    put_frame_buffer(out_dev, buffer);
    log_info("on_event");
    return 0;
}

int main(int argc, char* argv[])
{
    in_dev = init_frame_buffer_input_dev();
    out_dev = init_frame_buffer_output_dev();
    in_dev->on_event = on_event;
    out_dev->on_event = on_event;
    // fb_in_main_loop(in_dev);
    fb_out_main_loop(out_dev);
    return 0;
}