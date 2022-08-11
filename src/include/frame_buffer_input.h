#ifndef FRAME_BUFFER_INPUT_H
#define FRAME_BUFFER_INPUT_H

struct frame_buffer_input_data
{
	void *priv;
};

struct frame_buffer_input_dev
{
	char * name;
	int (* init)(struct frame_buffer_input_data *dev);
	int (* get_data)(struct frame_buffer_input_data *dev);
	int (* release)(struct frame_buffer_input_data *dev);
};
#endif