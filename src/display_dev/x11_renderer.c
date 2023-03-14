#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>

#include "util.h"
#include "display_dev.h"
#include "buffer_pool.h"
#include "input_event.h"

struct x11_renderer{
	Display *x_display;
	Window native_window;

	EGLDisplay display;
	EGLContext context;
	EGLSurface surface;
	EGLConfig config;

	uint32_t cur_buf_id;
	uint32_t fb_format;
	uint32_t fb_width;
	uint32_t fb_height;
	uint32_t frame_rate;
};

// int xxxmain(void) {
// 	Display *d;
// 	Window w;
// 	XEvent e;
// 	const char *msg = "Hello, World!";
// 	int s;
 
// 	d = XOpenDisplay(NULL);
// 	if (d == NULL) {
// 	  fprintf(stderr, "Cannot open display\n");
// 	  exit(1);
// 	}
 
// 	s = DefaultScreen(d);
// 	w = XCreateSimpleWindow(d, RootWindow(d, s), 10, 10, 100, 100, 1,
// 							BlackPixel(d, s), WhitePixel(d, s));
// 	XSelectInput(d, w, ExposureMask | KeyPressMask);
// 	XMapWindow(d, w);
 
// 	while (1) {
// 	  XNextEvent(d, &e);
// 	  if (e.type == Expose) {
// 		 XFillRectangle(d, w, DefaultGC(d, s), 20, 20, 10, 10);
// 		 XDrawString(d, w, DefaultGC(d, s), 10, 50, msg, strlen(msg));
// 	  }
// 	  if (e.type == KeyPress)
// 		 break;
// 	}
 
// 	XCloseDisplay(d);
// 	return 0;
// }

static int x11_renderer_init(struct display_object *obj)
{
	struct x11_renderer *priv;

	priv = calloc(1, sizeof(*priv));
	if(!priv)
	{
	  log_error("calloc fail, check free memery.");
	}

	priv->x_display = XOpenDisplay(NULL);
	if (priv->x_display == NULL) {
	  log_perr("Cannot open display\n");
	  return -1;
	}

	priv->display = eglGetDisplay(priv->x_display);
    if (priv->display == EGL_NO_DISPLAY)
    {
        goto FAIL1;
    }

    EGLint major, minor;
    if (!eglInitialize(priv->display, &major, &minor))
    {
        goto FAIL1;
    }

    const EGLint configAttribs[] = {
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE,    8,
        EGL_GREEN_SIZE,  8,
		EGL_BLUE_SIZE,   8,
		EGL_DEPTH_SIZE,  24,
		EGL_NONE};

    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};

    EGLint numConfigs;
    if (!eglChooseConfig(priv->display, configAttribs, &priv->config, 1,
    	&numConfigs))
    {
        goto FAIL1;
    }

    priv->context = eglCreateContext(priv->display, priv->config,
    	EGL_NO_CONTEXT, contextAttribs);
    if (priv->context == EGL_NO_CONTEXT)
    {
        goto FAIL1;
    }

    priv->frame_rate = 30;
	obj->priv = (void *)priv;
	return 0;

FAIL1:
	XCloseDisplay(priv->x_display);
	return -1;
}

static int x11_renderer_set_info(struct display_object *obj, GHashTable *fb_info)
{
	struct x11_renderer *priv = (struct x11_renderer *)obj->priv;

	priv->fb_width = *(uint32_t *)g_hash_table_lookup(fb_info, "width");
	priv->fb_height = *(uint32_t *)g_hash_table_lookup(fb_info, "height");
	priv->fb_format = *(uint32_t *)g_hash_table_lookup(fb_info, "format");

	if(!g_hash_table_contains(fb_info, "window"))
	{
	  log_error("x11_renderer_set_info:not find window.");
	  return -1;
	}

	priv->native_window = *(uint64_t *)g_hash_table_lookup(fb_info, "window");

	if(g_hash_table_contains(fb_info, "frame_rate"))
	  priv->frame_rate = *(uint32_t *)g_hash_table_lookup(fb_info, "frame_rate");

    priv->surface = eglCreateWindowSurface(priv->display, priv->config, priv->native_window, NULL);
    if (priv->surface == EGL_NO_SURFACE)
    {
        return -1;
    }

    if (!eglMakeCurrent(priv->display, priv->surface, priv->surface, 
    	priv->context))
    {
        goto FAIL1;
    }

	return 0;

FAIL1:
	eglDestroySurface(priv->display, priv->surface);
	return -1;
}

static int x11_renderer_map_buffer(struct display_object *obj, int buf_id)
{
	return 0;
}

static int x11_renderer_get_buffer(struct display_object *obj)
{
	struct x11_renderer *priv = (struct x11_renderer *)obj->priv;
	return priv->cur_buf_id;
}

static int x11_renderer_put_buffer(struct display_object *obj,
					int buf_id)
{
	struct raw_buffer *buffer;
	struct x11_renderer *priv = (struct x11_renderer *)obj->priv;

	buffer = get_raw_buffer(obj->buf_pool, buf_id);
	if(buffer == NULL)
	{
	  log_error("sdl get buffer fail! buf_id:%d", buf_id);
	  return -1;
	}

	switch(priv->fb_format)
	{
	case ARGB8888:
	  // SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptrs[0], priv->fb_width * 4);
	  break;
	case YUV420P:
	  break;
	case NV12:
	  // SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptrs[0], priv->fb_width);
	  break;
	default:
	  // SDL_UpdateTexture(priv->sdlTexture, NULL, buffer->ptrs[0], priv->fb_width * 4);
	  break;
	}

	eglSwapBuffers(priv->display, priv->surface);
	priv->cur_buf_id = buf_id;

	return 0;
}

static int x11_renderer_release(struct display_object *obj)
{
	struct x11_renderer *priv = (struct x11_renderer *)obj->priv;
	
	eglDestroyContext(priv->display, priv->context);
	eglDestroySurface(priv->display, priv->surface);
	XCloseDisplay(priv->x_display);

	free(priv);
	obj->priv = NULL;
	
	return 0;
}

struct display_dev_ops x11_renderer_ops =
{
	.name			= "x11_renderer",
	.init			= x11_renderer_init,
	.set_info		= x11_renderer_set_info,
	.map_buffer		= x11_renderer_map_buffer,
	.get_buffer		= x11_renderer_get_buffer,
	.put_buffer		= x11_renderer_put_buffer,
	.release		= x11_renderer_release,
};