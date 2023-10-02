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

struct x11_extensions {
	Display *display;
	uint32_t screen_num;
	Window root_win;
	XWindowAttributes windowattr;
	Visual *visual;
	bool quit;
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
	char * def_dpy = ":0.0";
	struct x11_extensions *priv;

	priv = calloc(1, sizeof(*priv));
	if(!priv) {
		log_error("calloc fail, check free memery.");
		goto FAIL1;
	}
	
	if(getenv("DISPLAY"))
		def_dpy=getenv("DISPLAY");

	priv->display = XOpenDisplay(def_dpy);

	if(priv->display == NULL) {
		log_error("XOpenDisplay: cannot connect to X server %s.",
				  XDisplayName(def_dpy));
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
	log_info("x11 xext informations of screen:%d width:%d height:%d.",
		priv->screen_num, priv->width, priv->height);

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

	if(priv->frame_rate > 120)
		priv->frame_rate = 120;
	
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

static int xext_map_fb(struct capture_object *obj, int buf_id) {
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

	xext_buf->shm.shmid = shmget(IPC_PRIVATE, (size_t)xext_buf->ximg->bytes_per_line * xext_buf->ximg->height, IPC_CREAT | 0600);

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
	raw_buf->ptrs[1] = NULL;
	raw_buf->ptrs[2] = NULL;
	raw_buf->ptrs[3] = NULL;

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

	buf_id = get_fb(obj->buf_pool);
	if(buf_id < 0) {
		log_error("get_fb fail.");
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

	if(put_fb(obj->buf_pool, buf_id) != 0) {
		log_error("put_fb fail.");
		exit(-1);
	}
	return 0;
}

static int xext_unmap_fb(struct capture_object *obj, int buf_id) {
	int ret;
	struct raw_buffer *raw_buf;
	struct xext_buf *xext_buf;
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	if(priv->xext_bufs[buf_id].ximg == NULL)
	{
		log_error("%s ximg is null.", __func__);
		return -1;
	}

	xext_buf = &priv->xext_bufs[buf_id];
	if (!XShmDetach(priv->display, &xext_buf->shm)) {
		log_error("XShmDetach failed.");
		exit(-1);
	}
	if((ret = shmdt(xext_buf->shm.shmaddr)))
	{
		log_error("shmdt failed:%d.", ret);
		exit(-1);
	}
	if((ret = shmctl(xext_buf->shm.shmid, IPC_RMID, 0)))
	{
		log_error("shmctl failed:%d.", ret);
		exit(-1);
	}
	if(!XDestroyImage(xext_buf->ximg))
	{
		log_error("XDestroyImage failed.");
		exit(-1);
	}
	xext_buf->ximg = NULL;

	raw_buf = get_raw_buffer(obj->buf_pool, buf_id);
	raw_buf->width = 0;
	raw_buf->hor_stride = 0;
	raw_buf->height = 0;
	raw_buf->ver_stride = 0;
	raw_buf->size = 0;
	raw_buf->ptrs[0] = NULL;
	return 0;
}

static int xext_dev_release(struct capture_object *obj) {
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;
	log_info("xext_dev_release");
	for (int i = 0; i < MAX_BUFFER_COUNT; ++i) {
		xext_unmap_fb(obj, i);
	}
	XSync(priv->display, False);
	XCloseDisplay(priv->display);
	free(priv);
	return 0;
}

static int xcb_main_loop(struct capture_object *obj)
{
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;
	static uint32_t times = 0;
	static uint32_t time_sum;
	uint64_t time_start, time_sub, time_perf_start;
	time_perf_start = get_time_us();
	while(!priv->quit)
	{
		time_start = get_time_us();
		obj->on_frame(obj);
		time_sub = get_time_us() - time_start;

		if(1000000 / priv->frame_rate > time_sub)
			usleep(1000000 / priv->frame_rate - time_sub);

		times++;
		if(times > 100)
		{
			time_sum = get_time_us() - time_perf_start;
			obj->fps = 1000000 * times / time_sum;
			times = 0;
			time_perf_start = get_time_us();
		}
	}

	return 0;
}

static int xcb_quit(struct capture_object *obj)
{
	struct x11_extensions *priv = (struct x11_extensions *)obj->priv;

	priv->quit = true;
	return 0;
}

struct capture_ops xcb_dev_ops = {
	.name 				= "x11_capture",
	.priority			= SHARE_MEMORY,
	.init 				= xext_dev_init,
	.set_info			= xext_set_info,
	.get_info			= xext_get_fb_info,
	.map_fb 			= xext_map_fb,
	.get_fb 			= xext_get_frame_buffer,
	.put_fb 			= xext_put_frame_buffer,
	.unmap_fb 			= xext_unmap_fb,
	.release 			= xext_dev_release,
	.main_loop			= xcb_main_loop,
	.quit				= xcb_quit
};

CAPTURE_ADD_DEV(99, xcb_dev_ops)