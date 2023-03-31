#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "common.h"

enum NET_CHANNEL
{
	VIDEO_CHANNEL,
	AUDIO_CHANNEL,
	INPUT_CHANNEL,
	SETTING_CHANNEL,
	USB_CHANNEL,
};

#pragma pack(push, 1)
struct data_pkt{
  enum NET_CHANNEL channel;
  uint32_t data_len;
  char data[0];
};
#pragma pack(pop)

#endif