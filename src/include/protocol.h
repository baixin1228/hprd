#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "common.h"

enum CMD
{
	VIDEO_DATA,
	AUDIO_DATA,
	INPUT_EVENT,
	SETTING_EVENT,
	USB_DATA,
};

#pragma pack(push, 1)
struct data_pkt{
  uint8_t cmd;
  uint32_t data_len;
  char data[0];
};
#pragma pack(pop)

#endif