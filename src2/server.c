#include <stdlib.h>

#include "util.h"
#include "output_dev.h"
#include "input_dev.h"

struct output_object * out_obj;
struct input_object * in_obj;
struct fb_info fb_info;

void on_event(struct output_object * obj)
{
	struct raw_buffer * fb = input_get_fb(in_obj);
	output_put_fb(obj, fb);
}

int main()
{
	out_obj = output_dev_init();
	in_obj = input_dev_init();
	output_regist_event_callback(out_obj, on_event);
	input_get_info(in_obj, &fb_info);
	output_set_info(out_obj, &fb_info);
	output_main_loop(out_obj);
	return 0;
}