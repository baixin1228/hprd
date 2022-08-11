#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include "module.h"
#include "util.h"
#include "frame_buffer_input.h"

struct x11_extensions
{
	Display* display;
	int screen_num;
	Window root_win;
	XWindowAttributes windowattr;
    Visual* visual;
    int depth;
	XShmSegmentInfo shm;
	XImage *xim;
};

static int xext_get_frame_buffer(struct frame_buffer_input_data *dev)
{
	struct x11_extensions *priv = (struct x11_extensions *)dev->priv;

	XShmGetImage(priv->display, priv->root_win, priv->xim,
		     0, 0, AllPlanes);
	XSync(priv->display, False);

	return 0;
}

#define DISPLAY_NAME ":0"
static int xext_dev_init(struct frame_buffer_input_data *dev)
{
	int ret = -1;
	struct x11_extensions *priv;

	priv = calloc(1, sizeof(*priv));
	if(!priv)
	{
		func_error("calloc fail, check free memery.");
	}

	priv->display = XOpenDisplay(DISPLAY_NAME);
    
    if(priv->display == NULL)
    {
        func_error("XOpenDisplay: cannot displayect to X server %s\n",
            XDisplayName(DISPLAY_NAME));
		goto FAIL1;
    }
	if(XShmQueryExtension(priv->display) == False)
	{
		goto FAIL2;
	}

	priv->screen_num = DefaultScreen(priv->display);
	priv->root_win = RootWindow(priv->display, priv->screen_num);

    if(XGetWindowAttributes(priv->display, priv->root_win,
        &priv->windowattr) == 0)
    {
        func_error("icvVideoRender: failed to get window attributes.\n");
		goto FAIL2;
    }

    priv->depth = priv->windowattr.depth;
    priv->visual = priv->windowattr.visual;
	priv->shm.shmid = -1;
	priv->shm.shmaddr = (char *) -1;
    int w = DisplayWidth(priv->display, priv->screen_num);
    int h = DisplayHeight(priv->display, priv->screen_num);
	func_error("x11 xext informations of screen:%d width:%d height:%d\n" , priv->screen_num
		, w , h);

	priv->xim = XShmCreateImage(priv->display, priv->visual, priv->depth, ZPixmap, NULL,
	    &priv->shm, w, h);

	if (priv->xim == NULL) {
		func_error("XShmCreateImage failed.\n");
		goto FAIL2;
	}

	priv->shm.shmid = shmget(IPC_PRIVATE,
	    (size_t)priv->xim->bytes_per_line * priv->xim->height, IPC_CREAT | 0600);

	if (priv->shm.shmid == -1) {
		func_error("shmget failed.\n");
		goto FAIL3;
	}

	priv->shm.readOnly = False;
	priv->shm.shmaddr = priv->xim->data = (char *) shmat(priv->shm.shmid, 0, 0);

	if (priv->shm.shmaddr == (char *)-1) {
		func_error("shmat failed.\n");
		goto FAIL4;
	}

	if (!XShmAttach(priv->display, &priv->shm)) {
		func_error("XShmAttach failed.\n");
		goto FAIL5;
	}
	XSync(priv->display, False);

	return 0;
FAIL5:
	shmdt(priv->shm.shmaddr);
FAIL4:
	shmctl(priv->shm.shmid, IPC_RMID, 0);
FAIL3:
	XDestroyImage(priv->xim);
FAIL2:
	XCloseDisplay(priv->display);
FAIL1:
	free(priv);
	return ret;
}

static int xext_dev_release(struct frame_buffer_input_data *dev)
{
	struct x11_extensions *priv = (struct x11_extensions *)dev->priv;

	XShmDetach(priv->display, &priv->shm);
	shmdt(priv->shm.shmaddr);
	shmctl(priv->shm.shmid, IPC_RMID, 0);
	XDestroyImage(priv->xim);
	XCloseDisplay(priv->display);
	free(priv);
	return 0;
}

struct frame_buffer_input_dev x11_extensions_input_dev = 
{
	.name 				= "x11_extensions_input_dev",
	.init 				= xext_dev_init,
	.get_data 		 	= xext_get_frame_buffer,
	.release 			= xext_dev_release
};

void xext_init3(void)
{
	printf("hhhhhhhhh2\n");
}
MODULE_INIT(xext_init3, FRAMEBUFFER_INPUT_DEV, DEVICE_PRIO_LOW);

void xext_init(void)
{
	printf("hhhhhhhhh\n");
}
MODULE_INIT(xext_init, FRAMEBUFFER_INPUT_DEV, DEVICE_PRIO_HEIGHT);

void xext_init2(void)
{
	printf("hhhhhhhhh2\n");
}
MODULE_INIT(xext_init2, FRAMEBUFFER_INPUT_DEV, DEVICE_PRIO_HEIGHT);