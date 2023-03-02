#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#include <glib.h>

#include "util.h"
#include "capture_dev.h"
#include "buffer_pool.h"
#include "frame_buffer.h"

#define DEFAULT_DISPLAY ":0"

struct x11_extensions {
	Display *display;
	uint32_t screen_num;
	Window root_win;
	XWindowAttributes windowattr;
	Visual *visual;
	uint32_t frame_rate;
	uint32_t format;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	struct xext_buf {
		int raw_buf_id;
		XImage *ximg;
		XShmSegmentInfo shm;
	} xext_bufs[MAX_BUFFER_COUNT];
	struct raw_buffer buffer;
};

static int xext_dev_init(struct capture_object *obj) {
	int ret = -1;
	struct x11_extensions *priv;

	priv = calloc(1, sizeof(*priv));
	if(!priv) {
		log_error("calloc fail, check free memery.");
		goto FAIL1;
	}

	priv->display = XOpenDisplay(DEFAULT_DISPLAY);

	if(priv->display == NULL) {
		log_error("XOpenDisplay: cannot displayect to X server %s.",
				  XDisplayName(DEFAULT_DISPLAY));
		goto FAIL2;
	}
	if(XShmQueryExtension(priv->display) == False) {
		goto FAIL3;
	}

	priv->screen_num = DefaultScreen(priv->display);
	priv->root_win = RootWindow(priv->display, priv->screen_num);

	if(XGetWindowAttributes(priv->display, priv->root_win,
							&priv->windowattr) == 0) {
		log_error("icvVideoRender: failed to get window attributes.");
		goto FAIL3;
	}

	priv->depth = priv->windowattr.depth;
	priv->visual = priv->windowattr.visual;

	priv->width = DisplayWidth(priv->display, priv->screen_num);
	priv->height = DisplayHeight(priv->display, priv->screen_num);
	log_info("x11 xext informations of screen:%d width:%d height:%d.", priv->screen_num
			 , priv->width, priv->height);

	priv->format = ARGB8888;
	priv->frame_rate = 33;

	obj->priv = (void *)priv;
	return 0;

FAIL3:
	XCloseDisplay(priv->display);
FAIL2:
	free(priv);
FAIL1:
	return ret;
}

static int xext_set_info(struct capture_object *obj, GHashTable *fb_info)
{
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	if(g_hash_table_contains(fb_info, "frame_rate"))
		priv->frame_rate = *(uint32_t *)g_hash_table_lookup(fb_info, "frame_rate");

	return 0;
}

static int xext_get_fb_info(struct capture_object *obj, GHashTable *fb_info) {
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	g_hash_table_insert(fb_info, "format", &priv->format);
	g_hash_table_insert(fb_info, "width", &priv->width);
	g_hash_table_insert(fb_info, "height", &priv->height);
	g_hash_table_insert(fb_info, "hor_stride", &priv->width);
	g_hash_table_insert(fb_info, "ver_stride", &priv->height);
	return 0;
}

static int xext_map_buffer(struct capture_object *obj, int buf_id) {
	struct raw_buffer *raw_buf;
	struct xext_buf *xext_buf;
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	if(priv->xext_bufs[buf_id].ximg != NULL)
		return -1;

	xext_buf = &priv->xext_bufs[buf_id];
	xext_buf->raw_buf_id = buf_id;
	xext_buf->ximg = XShmCreateImage(priv->display, priv->visual, priv->depth,
									 ZPixmap, NULL, &xext_buf->shm, priv->width, priv->height);

	if (xext_buf->ximg == NULL) {
		log_error("XShmCreateImage failed.");
		return -1;
	}

	xext_buf->shm.shmid = shmget(IPC_PRIVATE,
								 (size_t)xext_buf->ximg->bytes_per_line * xext_buf->ximg->height, IPC_CREAT | 0600);

	if (xext_buf->shm.shmid == -1) {
		log_error("shmget failed.");
		goto FAIL1;
	}

	xext_buf->shm.readOnly = False;
	xext_buf->shm.shmaddr = xext_buf->ximg->data = (char *) shmat(xext_buf->shm.shmid, 0, 0);

	if (xext_buf->shm.shmaddr == (char *) -1) {
		log_error("shmat failed.");
		goto FAIL2;
	}

	if (!XShmAttach(priv->display, &xext_buf->shm)) {
		log_error("XShmAttach failed.");
		goto FAIL3;
	}
	XSync(priv->display, False);

	raw_buf = get_raw_buffer(obj->buf_pool, buf_id);

	raw_buf->width = priv->width;
	raw_buf->hor_stride = priv->width;
	raw_buf->height = priv->height;
	raw_buf->ver_stride = priv->height;
	raw_buf->format = priv->format;
	raw_buf->bpp = 32;
	raw_buf->size = priv->width * priv->height * 4;
	raw_buf->ptrs[0] = xext_buf->ximg->data;
	raw_buf->ptrs[1] = malloc(1);
	raw_buf->ptrs[2] = malloc(1);
	raw_buf->ptrs[3] = malloc(1);

	return 0;

FAIL3:
	shmdt(xext_buf->shm.shmaddr);
FAIL2:
	shmctl(xext_buf->shm.shmid, IPC_RMID, 0);
FAIL1:
	XDestroyImage(xext_buf->ximg);
	return -1;
}

static int xext_get_frame_buffer(struct capture_object *obj) {
	int buf_id;
	struct xext_buf *xext_buf;
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	buf_id = get_buffer(obj->buf_pool);
	if(buf_id < 0) {
		log_error("get_buffer fail.");
		exit(-1);
	}

	xext_buf = &priv->xext_bufs[buf_id];

	XShmGetImage(priv->display, priv->root_win, xext_buf->ximg,
				 0, 0, AllPlanes);
	XSync(priv->display, False);
	return xext_buf->raw_buf_id;
}

static int xext_put_frame_buffer(struct capture_object *obj, int buf_id) {
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	if(priv->xext_bufs[buf_id].ximg == NULL)
		return -1;

	if(put_buffer(obj->buf_pool, buf_id) != 0) {
		log_error("put_buffer fail.");
		exit(-1);
	}
	return 0;
}

static int xext_unmap_buffer(struct capture_object *obj, int buf_id) {
	struct raw_buffer *raw_buf;
	struct xext_buf *xext_buf;
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	if(priv->xext_bufs[buf_id].ximg == NULL)
		return -1;

	xext_buf = &priv->xext_bufs[buf_id];
	xext_buf->ximg = NULL;
	shmdt(xext_buf->shm.shmaddr);
	shmctl(xext_buf->shm.shmid, IPC_RMID, 0);
	XDestroyImage(xext_buf->ximg);

	raw_buf = get_raw_buffer(obj->buf_pool, buf_id);
	raw_buf->width = 0;
	raw_buf->hor_stride = 0;
	raw_buf->height = 0;
	raw_buf->ver_stride = 0;
	raw_buf->size = 0;
	raw_buf->ptrs[0] = NULL;
	free(raw_buf->ptrs[1]);
	free(raw_buf->ptrs[2]);
	free(raw_buf->ptrs[3]);
	return 0;
}

static int xext_dev_release(struct capture_object *obj) {
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	for (int i = 0; i < MAX_BUFFER_COUNT; ++i) {
		xext_unmap_buffer(obj, i);
	}
	XCloseDisplay(priv->display);
	free(priv);
	return 0;
}

static int xcb_main_loop(struct capture_object *obj)
{
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	while(1)
	{
		obj->on_event(obj);
		usleep(1000000 / priv->frame_rate);
	}

	return 0;
}

struct capture_dev_ops xcb_dev_ops = {
	.name 				= "x11_extensions_capture_dev",
	.init 				= xext_dev_init,
	.set_info			= xext_set_info,
	.get_info			= xext_get_fb_info,
	.map_buffer 		= xext_map_buffer,
	.get_buffer 		= xext_get_frame_buffer,
	.put_buffer 		= xext_put_frame_buffer,
	.unmap_buffer 		= xext_unmap_buffer,
	.release 			= xext_dev_release,
	.event_loop			= xcb_main_loop
};