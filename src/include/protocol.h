#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "common.h"

enum CMD
{
	VIDEO_DATA,
	AUDIO_DATA,
	INPUT_EVENT,
	USB_DATA,
};

struct data_pkt{
  uint8_t cmd;
  size_t data_len;
  char data[0];
};

#endif