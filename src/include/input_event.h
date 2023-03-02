#ifndef __KEY_CODE_H__
#define __KEY_CODE_H__

#include "common.h"

enum event_type
{
	MOUSE_MOVE,
	KEY_UP,
	KEY_DOWN,
	MOUSE_UP,
	MOUSE_DOWN,
	BUTTON_UP,
	BUTTON_DOWN,
};

struct input_event{
  uint8_t type;
  uint8_t state;
  uint32_t key_code;
  uint32_t x;
  uint32_t y;
};

int send_event(struct input_event *event);

#endif