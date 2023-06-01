#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "common.h"
#include "net/net_server.h"

enum NET_CHANNEL
{
	VIDEO_CHANNEL,
	AUDIO_CHANNEL,
	INPUT_CHANNEL,
	CLIP_CHANNEL,
	REQUEST_CHANNEL,
	RESPONSE_CHANNEL,
	USB_CHANNEL,
};

#pragma pack(push, 1)
struct data_pkt{
  enum NET_CHANNEL channel;
  uint32_t data_len;
  char data[0];
};
#pragma pack(pop)


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

struct clip_event{
  uint16_t data_len;
  char clip_data[0];
};

#define RET_SUCCESS 1
#define RET_FAIL 2
enum request_cmd
{
	SET_BIT_RATE,
	SET_FRAME_RATE,
	SET_SHARE_CLIPBOARD,

	GET_BIT_RATE,
	GET_FRAME_RATE,
	GET_FPS,
	GET_CLIENT_ID,
	PING
};

struct request_event{
  uint8_t cmd;
  uint32_t id;
  uint32_t value;
};

struct response_event{
  uint8_t ret;
  uint32_t id;
  uint32_t value;
};

int send_input_event(char *buf, size_t len);
int send_request_event(uint32_t cmd, uint32_t value, uint32_t id);
int send_clip_event(char *type, char *data, uint16_t len);
int server_send_event(struct server_client *client, uint32_t cmd, char *buf,
	size_t len);
void bradcast_video(char *buf, size_t len);

#endif