#include <glib.h>
#include <stdio.h>

#include "SDL2/SDL.h"

#include "util.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "input_event.h"

struct sdl_fd_out
{
	SDL_Window *screen;
	SDL_Renderer *sdlRenderer;
	SDL_Texture *sdlTexture;
	SDL_Thread *refresh_thread;
	SDL_Rect sdlRect;
	uint32_t fb_width;
	uint32_t fb_height;
	uint32_t linesize;
	uint32_t frame_rate;
	int screen_w;
	int screen_h;
	int fb_format;
	int cur_buf_id;
	int mouse_pos_x;
	int mouse_pos_y;
};

void _poll_event(struct sdl_fd_out *priv)
{
	SDL_Event event;

	/* One-time consumption of all events */
	while(true)
	{
		memset(&event, 0, sizeof(SDL_Event));
		SDL_PollEvent(&event);

		switch(event.type)
		{
			case SDL_WINDOWEVENT:
			{
				if(priv->screen)
					SDL_GetWindowSize(priv->screen, &priv->screen_w,
						&priv->screen_h);
				break;
			}
			case SDL_QUIT:
			{
				exit(0);
				break;
			}
			case SDL_MOUSEMOTION:
			{
				priv->mouse_pos_x = event.motion.x;
				priv->mouse_pos_y = event.motion.y;
				break;
			}
			case SDL_WINDOWEVENT_NONE:
			default:
			{
				return;
			}
		}
	}
}

static void* _poll_event_thread(void *oqu)
{
	struct sdl_fd_out *priv = oqu;

	while(true)
	{
		_poll_event(priv);
		SDL_Delay(1000 / priv->frame_rate);
	}
	return NULL;
}

static int sdl_dev_init(struct display_object *obj)
{
	struct sdl_fd_out *priv;
	pthread_t p1;

	priv = calloc(1, sizeof(*priv));
	if(!priv)
	{
		log_error("calloc fail, check free memery.");
	}

	if(SDL_Init(SDL_INIT_VIDEO))
	{
		printf( "Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	priv->frame_rate = 33;

	pthread_create(&p1, NULL, _poll_event_thread, priv);
	pthread_detach(p1);

	obj->priv = (void *)priv;

	return 0;
}

static int _com_fmt_to_sdl_fmt(enum FRAMEBUFFER_FORMAT format)
{
	switch(format)
	{
	case ARGB8888:
		return SDL_PIXELFORMAT_ARGB8888;
		break;
	case YUV420P:
		return SDL_PIXELFORMAT_IYUV;
		break;
	case NV12:
		return SDL_PIXELFORMAT_NV12;
		break;
	default:
		return SDL_PIXELFORMAT_ARGB8888;
		break;
	}
}

static int sdl_set_info(struct display_object *obj, GHashTable *fb_info)
{
	int fmt;
	int sdl_fmt;
	struct sdl_fd_out *priv = (struct sdl_fd_out *)obj->priv;

	priv->fb_width = *(uint32_t *)g_hash_table_lookup(fb_info, "width");
	priv->fb_height = *(uint32_t *)g_hash_table_lookup(fb_info, "height");
	priv->screen_w = *(uint32_t *)g_hash_table_lookup(fb_info, "width") / 2;
	priv->screen_h = *(uint32_t *)g_hash_table_lookup(fb_info, "height") / 2;
	fmt = *(uint32_t *)g_hash_table_lookup(fb_info, "format");
	sdl_fmt = _com_fmt_to_sdl_fmt(fmt);

	if(g_hash_table_contains(fb_info, "frame_rate"))
		priv->frame_rate = *(uint32_t *)g_hash_table_lookup(fb_info, "frame_rate");

	//SDL 2.0 Support for multiple windows
	priv->screen = SDL_CreateWindow("Simplest Video Play SDL2",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		priv->screen_w, priv->screen_h,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if(!priv->screen)
	{
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}
	priv->sdlRenderer = SDL_CreateRenderer(priv->screen, -1,
		SDL_RENDERER_ACCELERATED);

	priv->sdlTexture = SDL_CreateTexture(priv->sdlRenderer, sdl_fmt,
		SDL_TEXTUREACCESS_STREAMING,
		priv->fb_width, priv->fb_height);

	// priv->refresh_thread = SDL_CreateThread(refresh_video,
	// 	"SDL_refresh_thread", 
	// 	&priv->frame_rate);

	priv->sdlRect.x = 0;
	priv->sdlRect.y = 0;
	priv->sdlRect.w = priv->screen_w;
	priv->sdlRect.h = priv->screen_h;
	priv->fb_format = *(uint32_t *)g_hash_table_lookup(fb_info, "format");
	priv->cur_buf_id = -1;
	return 0;
}

static int sdl_map_buffer(struct display_object *obj, int buf_id)
{
	return 0;
}

static int sdl_get_buffer(struct display_object *obj)
{
	struct sdl_fd_out *priv = (struct sdl_fd_out *)obj->priv;
	return priv->cur_buf_id;
}

static int sdl_put_buffer(struct display_object *obj,
						  int buf_id)
{
	struct raw_buffer *buffer;
	struct sdl_fd_out *priv = (struct sdl_fd_out *)obj->priv;

	buffer = get_raw_buffer(obj->buf_pool, buf_id);
	if(buffer == NULL)
	{
		log_error("sdl get buffer fail! buf_id:%d", buf_id);
		return -1;
	}

	if(!priv->sdlTexture)
	{
		log_error("sdl texture is none!");
		return -1;
	}

	switch(priv->fb_format)
	{
	case ARGB8888:
		SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptrs[0], priv->fb_width * 4);
		break;
	case YUV420P:
		SDL_UpdateYUVTexture(priv->sdlTexture, NULL,
			     (uint8_t *)buffer->ptrs[0], buffer->hor_stride,
			     (uint8_t *)buffer->ptrs[1], buffer->hor_stride / 2,
			     (uint8_t *)buffer->ptrs[2], buffer->hor_stride / 2);
		break;
	case NV12:
		SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptrs[0], priv->fb_width);
		break;
	default:
		SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptrs[0], priv->fb_width * 4);
		break;
	}

	SDL_RenderClear(priv->sdlRenderer);
	SDL_RenderCopy(priv->sdlRenderer, priv->sdlTexture, NULL, &priv->sdlRect);
	SDL_RenderPresent(priv->sdlRenderer);
	priv->cur_buf_id = buf_id;

	return 0;
}

static int sdl_main_loop(struct display_object *obj)
{
	struct sdl_fd_out *priv = (struct sdl_fd_out *)obj->priv;
	struct input_event event;
	
	log_info("sdl_main_loop");

	while(true)
	{
		memset(&event, 0, sizeof(struct input_event));
		event.type = MOUSE_MOVE;
		event.x = priv->mouse_pos_x;
		event.y = priv->mouse_pos_y;
		hsend_event(&event);

		obj->on_event(obj);
		SDL_Delay(1000 / priv->frame_rate);
	}

	return 0;
}

struct display_dev_ops sdl_ops =
{
	.name				= "sdl_fb_out",
	.init				= sdl_dev_init,
	.set_info			= sdl_set_info,
	.map_buffer			= sdl_map_buffer,
	.get_buffer			= sdl_get_buffer,
	.put_buffer			= sdl_put_buffer,
	.event_loop			= sdl_main_loop,
};