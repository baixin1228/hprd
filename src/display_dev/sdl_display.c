#include <glib.h>
#include <stdio.h>

#include "SDL2/SDL.h"

#include "util.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "input_event.h"

struct sdl_fd_out
{
	SDL_Window *sdl_window;
	SDL_Renderer *sdl_renderer;
	SDL_Texture *sdl_texture;
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
	struct input_event event;
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
				if(priv->sdl_window)
					SDL_GetWindowSize(priv->sdl_window, &priv->screen_w,
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
				if(priv->event.type == 0)
				{
					priv->event.type = MOUSE_MOVE;
					priv->event.x = event.motion.x;
					priv->event.y = event.motion.y;
				}
				break;
			}
			case SDL_MOUSEBUTTONDOWN:
			{
				if(priv->event.type == 0)
				{
					priv->event.type = MOUSE_DOWN;
					priv->event.key_code = event.button.button;

					priv->event.x = event.motion.x;
					priv->event.y = event.motion.y;
				}
				break;
			}
			case SDL_MOUSEBUTTONUP:
			{
				if(priv->event.type == 0)
				{
					priv->event.type = MOUSE_UP;
					priv->event.key_code = event.button.button;
					
					priv->event.x = event.motion.x;
					priv->event.y = event.motion.y;
				}
				break;
			}
			case SDL_MOUSEWHEEL:
			{
				if(priv->event.type == 0)
				{
					priv->event.type = MOUSE_WHEEL;
					if(event.wheel.y > 0)
					{
						priv->event.key_code = 4;
					}
					else if(event.wheel.y < 0)
					{
						priv->event.key_code = 5;
					}

					if(event.wheel.x < 0)
					{
						priv->event.key_code = 6;
					}
					else if(event.wheel.x > 0)
					{
						priv->event.key_code = 7;
					}
				}
				break;
			}
			case SDL_KEYDOWN:
			{
				printf("------------- code:%d %d\n", event.key.keysym.scancode, event.key.keysym.sym);
				if(priv->event.type == 0)
				{
					priv->event.type = KEY_DOWN;
					if(event.key.keysym.sym == 1073742048)
						event.key.keysym.sym = 17;
					priv->event.key_code = event.key.keysym.sym;
				}
				break;
			}
			case SDL_KEYUP:
			{
				if(priv->event.type == 0)
				{
					priv->event.type = KEY_UP;
					priv->event.key_code = event.key.keysym.sym;
				}
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
	
#ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");

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
	priv->screen_w = *(uint32_t *)g_hash_table_lookup(fb_info, "width");
	priv->screen_h = *(uint32_t *)g_hash_table_lookup(fb_info, "height");
	fmt = *(uint32_t *)g_hash_table_lookup(fb_info, "format");
	sdl_fmt = _com_fmt_to_sdl_fmt(fmt);

	if(g_hash_table_contains(fb_info, "frame_rate"))
		priv->frame_rate = *(uint32_t *)g_hash_table_lookup(fb_info, "frame_rate");

	//SDL 2.0 Support for multiple windows
	priv->sdl_window = SDL_CreateWindow("Simplest Video Play SDL2",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		priv->screen_w, priv->screen_h,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if(!priv->sdl_window)
	{
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}
	priv->sdl_renderer = SDL_CreateRenderer(priv->sdl_window, -1,
		SDL_RENDERER_ACCELERATED);

	priv->sdl_texture = SDL_CreateTexture(priv->sdl_renderer, sdl_fmt,
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
		printf("buffer,w:%d h:%d h:%d v:%d\n", buffer->width, buffer->height, buffer->hor_stride, buffer->ver_stride);

	if(!priv->sdl_texture)
	{
		log_error("sdl texture is none!");
		return -1;
	}

	switch(priv->fb_format)
	{
	case ARGB8888:
		SDL_UpdateTexture(priv->sdl_texture, NULL, buffer->ptrs[0], priv->fb_width * 4);
		break;
	case YUV420P:
		SDL_UpdateYUVTexture(priv->sdl_texture, NULL,
			     (uint8_t *)buffer->ptrs[0], buffer->hor_stride,
			     (uint8_t *)buffer->ptrs[1], buffer->hor_stride / 2,
			     (uint8_t *)buffer->ptrs[2], buffer->hor_stride / 2);
		break;
	case NV12:
		SDL_UpdateTexture(priv->sdl_texture, NULL, buffer->ptrs[0], priv->fb_width);
		break;
	default:
		SDL_UpdateTexture(priv->sdl_texture, NULL, buffer->ptrs[0], priv->fb_width * 4);
		break;
	}

	SDL_RenderClear(priv->sdl_renderer);
	SDL_RenderCopy(priv->sdl_renderer, priv->sdl_texture, NULL,
		&priv->sdlRect);
	SDL_RenderPresent(priv->sdl_renderer);
	priv->cur_buf_id = buf_id;

	return 0;
}

static int sdl_main_loop(struct display_object *obj)
{
	struct sdl_fd_out *priv = (struct sdl_fd_out *)obj->priv;
	
	log_info("sdl_main_loop");

	while(true)
	{
		if(priv->event.type != 0)
		{
			if(obj->on_event != NULL)
				obj->on_event(obj, &priv->event);

			priv->event.type = 0;
		}

		obj->on_frame(obj);
		SDL_Delay(1000 / priv->frame_rate);
	}

	return 0;
}


static int sdl_release(struct display_object *obj)
{
	struct sdl_fd_out *priv = (struct sdl_fd_out *)obj->priv;
	
	SDL_DestroyTexture(priv->sdl_texture);
	SDL_DestroyRenderer(priv->sdl_renderer);
	SDL_DestroyWindow(priv->sdl_window);
	free(priv);
	obj->priv = NULL;
	
	return 0;
}


struct display_dev_ops sdl_ops =
{
	.name				= "sdl_display",
	.priority 			= 10,
	.init				= sdl_dev_init,
	.set_info			= sdl_set_info,
	.map_buffer			= sdl_map_buffer,
	.get_buffer			= sdl_get_buffer,
	.put_buffer			= sdl_put_buffer,
	.main_loop			= sdl_main_loop,
	.release			= sdl_release,
};