#include <stdlib.h>

#include "util.h"
#include "output_dev.h"
#include "input_dev.h"

struct output_object * out_obj;
struct input_object * in_obj;

void on_event()
{

}

int main()
{
	out_obj = output_dev_init();
	in_obj = input_dev_init();
	// out_obj->event = on_event;
	// output_set_info(out_obj, );
	return 0;
}