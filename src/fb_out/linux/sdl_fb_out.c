#include <stdio.h>
#include "SDL2/SDL.h"

#include "module.h"
#include "util.h"
#include "fb_out.h"

struct sdl_fd_out
{
	SDL_Window *screen; 
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Thread *refresh_thread;
	SDL_Rect sdlRect;
	uint32_t fb_width;
	uint32_t fb_height;
	uint32_t linesize;
	int screen_w;
	int screen_h;
	int fb_format;
};


//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)

int thread_exit=0;

int refresh_video(void *opaque){
	while (thread_exit==0) {
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(25);
	}
	return 0;
}

static int sdl_dev_init(struct module_data *dev)
{
	struct sdl_fd_out *priv;

	priv = calloc(1, sizeof(*priv));
	if(!priv)
	{
		func_error("calloc fail, check free memery.");
	}

	if(SDL_Init(SDL_INIT_VIDEO)) {  
		printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
		return -1;
	}

	dev->priv = (void *)priv;
	
	return 0;
}

static int _com_fb_fmt_to_sdl_fmt(enum COMMON_BUFFER_FORMAT format)
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

static int sdl_set_fb_info(struct module_data *dev, struct frame_buffer_info fb_info)
{
	struct sdl_fd_out *priv = (struct sdl_fd_out *)dev->priv;

	priv->fb_width = fb_info.width;
	priv->fb_height = fb_info.height;

	priv->screen_w = fb_info.width / 2;
	priv->screen_h = fb_info.height / 2;
	//SDL 2.0 Support for multiple windows
	priv->screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		priv->screen_w, priv->screen_h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	if(!priv->screen) {  
		printf("SDL: could not create window - exiting:%s\n",SDL_GetError());  
		return -1;
	}
	priv->sdlRenderer = SDL_CreateRenderer(priv->screen, -1, 0);

	priv->sdlTexture = SDL_CreateTexture(priv->sdlRenderer, _com_fb_fmt_to_sdl_fmt(fb_info.format), 
		SDL_TEXTUREACCESS_STREAMING, priv->fb_width, priv->fb_height);

	priv->refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);

	priv->sdlRect.x = 0;
	priv->sdlRect.y = 0;
	priv->sdlRect.w = priv->screen_w;
	priv->sdlRect.h = priv->screen_h;
	priv->fb_format = fb_info.format;
	return 0;
}

static int sdl_put_buffer(struct module_data *dev, struct common_buffer *buffer)
{
	struct sdl_fd_out *priv = (struct sdl_fd_out *)dev->priv;

	if(!priv->sdlTexture)
	{
		func_error("sdl texture is none!");
		return -1;
	}

	switch(priv->fb_format)
	{
		case ARGB8888:
			SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptr, priv->fb_width * 4);
		break;
		case YUV420P:
			// SDL_UpdateYUVTexture(priv->sdlTexture, NULL,
			// 	     buffer->ptr, priv->fb_width,
			// 	     buffer->ptr + priv->fb_width * priv->fb_height, priv->fb_width / 2,
			// 	     buffer->ptr + priv->fb_width * priv->fb_height * 5 / 4
			// 	     , priv->fb_width / 2);

			SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptr, priv->fb_width);
		break;
		case NV12:
			SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptr, priv->fb_width);
		break;
		default:
			SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptr, priv->fb_width * 4);
		break;
	}

	SDL_RenderClear(priv->sdlRenderer);
	SDL_RenderCopy(priv->sdlRenderer, priv->sdlTexture, NULL, &priv->sdlRect);
	SDL_RenderPresent(priv->sdlRenderer);
	return 0;
}

static int sdl_main_loop(struct module_data *dev)
{
	SDL_Event event;
	struct sdl_fd_out *priv = (struct sdl_fd_out *)dev->priv;
	
	log_info("sdl_main_loop");

	while(1)
	{
		SDL_WaitEvent(&event);
		if(event.type==REFRESH_EVENT)
		{
			dev->on_event();
		}else if(event.type==SDL_WINDOWEVENT){
			/* If Resize */
			SDL_GetWindowSize(priv->screen,&priv->screen_w,&priv->screen_h);
		}else if(event.type==SDL_QUIT){
			exit(0);
		}
	}

	return 0;
}

struct fb_out_ops sdl_fb_out_dev = 
{
	.name				= "sdl_fb_out",
	.init				= sdl_dev_init,
	.set_fb_info		= sdl_set_fb_info,
	.put_buffer			= sdl_put_buffer,
	.main_loop			= sdl_main_loop,
};

FRAME_BUFFER_OUTPUT_DEV(sdl_fb_out_dev, DEVICE_PRIO_LOW);