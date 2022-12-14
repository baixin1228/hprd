#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include "util.h"
#include "buffer.h"
#include "module.h"
#include "fb_in.h"

#define DEFAULT_DISPLAY ":0"

struct x11_extensions
{
	Display* display;
	int screen_num;
	Window root_win;
	XWindowAttributes windowattr;
	Visual* visual;
	int depth;
	XShmSegmentInfo shm;
	XImage *ximg;
	struct raw_buffer buffer;
	uint32_t fb_idx;
};

static struct raw_buffer * xext_get_frame_buffer(struct input_objct *obj)
{
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;
	XShmGetImage(priv->display, priv->root_win, priv->ximg,
		0, 0, AllPlanes);
	XSync(priv->display, False);
	priv->fb_idx++;
	priv->buffer.id = priv->fb_idx;
	return &priv->buffer;
}

static int xext_dev_init(struct input_objct *obj)
{
	int ret = -1;
	struct x11_extensions *priv;

	priv = calloc(1, sizeof(*priv));
	if(!priv)
	{
		func_error("calloc fail, check free memery.");
		goto FAIL1;
	}

	priv->display = XOpenDisplay(DEFAULT_DISPLAY);

	if(priv->display == NULL)
	{
		func_error("XOpenDisplay: cannot displayect to X server %s.",
			XDisplayName(DEFAULT_DISPLAY));
		goto FAIL2;
	}
	if(XShmQueryExtension(priv->display) == False)
	{
		goto FAIL3;
	}

	priv->screen_num = DefaultScreen(priv->display);
	priv->root_win = RootWindow(priv->display, priv->screen_num);

	if(XGetWindowAttributes(priv->display, priv->root_win,
		&priv->windowattr) == 0)
	{
		func_error("icvVideoRender: failed to get window attributes.");
		goto FAIL3;
	}

	priv->depth = priv->windowattr.depth;
	priv->visual = priv->windowattr.visual;
	priv->shm.shmid = -1;
	priv->shm.shmaddr = (char *) -1;
	int w = DisplayWidth(priv->display, priv->screen_num);
	int h = DisplayHeight(priv->display, priv->screen_num);
	log_info("x11 xext informations of screen:%d width:%d height:%d." , priv->screen_num
		, w , h);

	priv->ximg = XShmCreateImage(priv->display, priv->visual, priv->depth, ZPixmap, NULL,
		&priv->shm, w, h);

	if (priv->ximg == NULL) {
		func_error("XShmCreateImage failed.");
		goto FAIL3;
	}

	priv->shm.shmid = shmget(IPC_PRIVATE,
		(size_t)priv->ximg->bytes_per_line * priv->ximg->height, IPC_CREAT | 0600);

	if (priv->shm.shmid == -1) {
		func_error("shmget failed.");
		goto FAIL4;
	}

	priv->shm.readOnly = False;
	priv->shm.shmaddr = priv->ximg->data = (char *) shmat(priv->shm.shmid, 0, 0);

	if (priv->shm.shmaddr == (char *)-1) {
		func_error("shmat failed.");
		goto FAIL5;
	}

	if (!XShmAttach(priv->display, &priv->shm)) {
		func_error("XShmAttach failed.");
		goto FAIL6;
	}
	XSync(priv->display, False);

	priv->buffer.width = w;
	priv->buffer.hor_stride = w;
	priv->buffer.height = h;
	priv->buffer.ver_stride = h;
	priv->buffer.format = ARGB8888;
	priv->buffer.bpp = 32;
	priv->buffer.size = w * h * 4;
	priv->buffer.ptr = (uint8_t *)priv->ximg->data;

	priv->fb_idx = 0;

	obj->priv = (void *)priv;
	return 0;
FAIL6:
	shmdt(priv->shm.shmaddr);
FAIL5:
	shmctl(priv->shm.shmid, IPC_RMID, 0);
FAIL4:
	XDestroyImage(priv->ximg);
FAIL3:
	XCloseDisplay(priv->display);
FAIL2:
	free(priv);
FAIL1:
	return ret;
}

static int xext_get_fb_info(struct input_objct *obj, struct frame_buffer_info *fb_info)
{
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	fb_info->format = priv->buffer.format;
	fb_info->width = priv->buffer.width;
	fb_info->height = priv->buffer.height;
	fb_info->hor_stride = priv->buffer.hor_stride;
	fb_info->ver_stride = priv->buffer.ver_stride;
	return 0;
}

static int xext_dev_release(struct input_objct *obj)
{
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	XShmDetach(priv->display, &priv->shm);
	shmdt(priv->shm.shmaddr);
	shmctl(priv->shm.shmid, IPC_RMID, 0);
	XDestroyImage(priv->ximg);
	XCloseDisplay(priv->display);
	free(priv);
	return 0;
}

struct input_dev_ops x11_extensions_input_dev = 
{
	.name 				= "x11_extensions_input_dev",
	.init 				= xext_dev_init,
	.get_fb_info		= xext_get_fb_info,
	.get_data 		 	= xext_get_frame_buffer,
	.release 			= xext_dev_release
};