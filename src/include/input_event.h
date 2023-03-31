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

enum setting_cmd
{
	RET_SUCCESS = 1,
	RET_FAIL,
	SET_BIT_RATE,
	SET_FRAME_RATE,

	GET_BIT_RATE,
	GET_FRAME_RATE,
};

struct setting_event{
  uint8_t cmd;
  uint32_t value;
};

int send_event(int fd, uint32_t cmd, char *buf, size_t len);

#endif