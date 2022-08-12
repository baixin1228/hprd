#include <stdio.h>
#include "SDL2/SDL.h"

#include "module.h"
#include "util.h"
#include "fb_out/fb_out.h"

//set '1' to choose a type of file to play
#define LOAD_BGRA    1
#define LOAD_RGB24   0
#define LOAD_BGR24   0
#define LOAD_YUV420P 0

//Bit per Pixel
#if LOAD_BGRA
#define bpp 32
#elif LOAD_RGB24|LOAD_BGR24
#define bpp 24
#elif LOAD_YUV420P
#define bpp 12
#endif

int screen_w=1200,screen_h=500;
#define pixel_w 4480
#define pixel_h 1440

//BPP=32
unsigned char buffer_convert[pixel_w*pixel_h*4];

struct sdl_fd_out
{
	SDL_Window *screen; 
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Thread *refresh_thread;
};

void CONVERT_24to32(unsigned char *image_in,unsigned char *image_out,int w,int h){
	for(int i =0;i<h;i++)
		for(int j=0;j<w;j++){
			//Big Endian or Small Endian?
			//"ARGB" order:high bit -> low bit.
			//ARGB Format Big Endian (low address save high MSB, here is A) in memory : A|R|G|B
			//ARGB Format Little Endian (low address save low MSB, here is B) in memory : B|G|R|A
			if(SDL_BYTEORDER==SDL_LIL_ENDIAN){
				//Little Endian (x86): R|G|B --> B|G|R|A
				image_out[(i*w+j)*4+0]=image_in[(i*w+j)*3+2];
				image_out[(i*w+j)*4+1]=image_in[(i*w+j)*3+1];
				image_out[(i*w+j)*4+2]=image_in[(i*w+j)*3];
				image_out[(i*w+j)*4+3]='0';
			}else{
				//Big Endian: R|G|B --> A|R|G|B
				image_out[(i*w+j)*4]='0';
				memcpy(image_out+(i*w+j)*4+1,image_in+(i*w+j)*3,3);
			}
		}
}


//Refresh Event
#define REFRESH_EVENT  (SDL_USEREVENT + 1)

int thread_exit=0;

int refresh_video(void *opaque){
	while (thread_exit==0) {
		SDL_Event event;
		event.type = REFRESH_EVENT;
		SDL_PushEvent(&event);
		SDL_Delay(33);
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

	//SDL 2.0 Support for multiple windows
	priv->screen = SDL_CreateWindow("Simplest Video Play SDL2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	if(!priv->screen) {  
		printf("SDL: could not create window - exiting:%s\n",SDL_GetError());  
		return -1;
	}
	priv->sdlRenderer = SDL_CreateRenderer(priv->screen, -1, 0);

	priv->sdlTexture = SDL_CreateTexture(priv->sdlRenderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING,pixel_w,pixel_h);

	priv->refresh_thread = SDL_CreateThread(refresh_video,NULL,NULL);
	
	dev->priv = (void *)priv;
	
	return 0;
}

static int sdl_put_buffer(struct module_data *dev, struct common_buffer *buffer)
{
	SDL_Rect sdlRect;
	struct sdl_fd_out *priv = (struct sdl_fd_out *)dev->priv;

	SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptr, pixel_w*4);  

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	SDL_RenderClear(priv->sdlRenderer);
	SDL_RenderCopy(priv->sdlRenderer, priv->sdlTexture, NULL, &sdlRect);
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
			SDL_GetWindowSize(priv->screen,&screen_w,&screen_h);
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
	.put_buffer			= sdl_put_buffer,
	.main_loop			= sdl_main_loop,
};

FRAME_BUFFER_OUTPUT_DEV(sdl_fb_out_dev, DEVICE_PRIO_LOW);