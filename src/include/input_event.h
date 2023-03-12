#ifndef __KEY_CODE_H__
#define __KEY_CODE_H__

#include "common.h"

enum event_type
{
	MOUSE_MOVE = 1,
	KEY_UP,
	KEY_DOWN,
	MOUSE_UP,
	MOUSE_DOWN,
	MOUSE_WHEEL,
	BUTTON_UP,
	BUTTON_DOWN,
};

enum event_key
{
	MOUSE_LEFT,
	MOUSE_RIGHT,
};

struct input_event{
  uint8_t type;
  uint8_t state;
  uint32_t key_code;
  uint32_t x;
  uint32_t y;
};

int hsend_event(struct input_event *event);

#endif