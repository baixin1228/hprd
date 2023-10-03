#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/extensions/XTest.h>

#include <glib.h>

#include "util.h"
#include "input_dev.h"
#include "protocol.h"

#define DEFAULT_DISPLAY ":0"

struct xlib_input {
	Display *display;
	Atom sel;
	char clip_type[64];
	char clip_data[10240];
	size_t clip_len;
	uint32_t screen_num;
	Window root_win;
	pthread_t clip_thread;
};

int _on_request_atom(struct xlib_input *priv, XSelectionRequestEvent *req)
{
	if(strcmp("text/plain", priv->clip_type) == 0)
	{
		Atom string_atom_list[] = { 
			XInternAtom(priv->display, "UTF8_STRING", False),
			XInternAtom(priv->display, "TEXT", False),
			XInternAtom(priv->display, "STRING", False),
			XInternAtom(priv->display, "text/plain", False),
			XInternAtom(priv->display, "COMPOUND_TEXT", False),
			XA_STRING
		};
		XChangeProperty(priv->display, req->requestor,
					req->property, XInternAtom(priv->display, "ATOM", False),
					32, PropModeReplace,
					(uint8_t *)string_atom_list,
					sizeof(string_atom_list) / sizeof(string_atom_list[0]));
		return 0;
	}
	log_warning("[%s] not find clip type:%s", __func__, priv->clip_type);
	return -1;
}

void _on_selection_request(struct xlib_input *priv, XSelectionRequestEvent *req)
{
	XEvent respond = {0};

	Atom targets = XInternAtom(priv->display, "TARGETS", False);
	if(req->target == targets) {
		if(_on_request_atom(priv, req) != 0)
			return;
	} else {
		XChangeProperty(priv->display,
						req->requestor,
						req->property, req->target,
						8,
						PropModeReplace,
						(unsigned char *)priv->clip_data,
						priv->clip_len);
		log_info("clip board set:[%s]", __func__, priv->clip_data);
	}

	respond.xselection.property = req->property;
	respond.xselection.type = SelectionNotify;
	respond.xselection.display = req->display;
	respond.xselection.requestor = req->requestor;
	respond.xselection.selection = req->selection;
	respond.xselection.target = req->target;
	respond.xselection.time = req->time;
	XSendEvent(priv->display, req->requestor, 0, 0, &respond);
	XFlush(priv->display);
}

void show_utf8_prop(Display *dpy, Window w, Atom p)
{
	Atom da, incr, type;
	int di;
	unsigned long size, dul;
	unsigned char *prop_ret = NULL;
 
	/* Dummy call to get type and size. */
	XGetWindowProperty(dpy, w, p, 0, 0, False, AnyPropertyType,
					   &type, &di, &dul, &size, &prop_ret);
	XFree(prop_ret);
 
	incr = XInternAtom(dpy, "INCR", False);
	if (type == incr)
	{
		log_error("Data too large and INCR mechanism not implemented\n");
		return;
	}
 
 	if(size > 0)
 	{
		/* Read the data in one go. */
		printf("Property size: %lu\n", size);
	 	XGetWindowProperty(dpy, w, p, 0, size, False, AnyPropertyType,
						   &da, &di, &dul, &dul, &prop_ret);
		printf("%s\n", prop_ret);
		fflush(stdout);
		XFree(prop_ret);
 	}
 
	/* Signal the selection owner that we have successfully read the
	 * data. */
	XDeleteProperty(dpy, w, p);
}

void * _clip_server(void *oqu)
{
	XEvent ev;
	Atom property;
	XSelectionRequestEvent *serqv;
	XSelectionEvent *sev;
	struct xlib_input *priv = (struct xlib_input *)oqu;

	log_info("clip server start.");
	priv->sel = XInternAtom(priv->display, "CLIPBOARD", False);
	property = XInternAtom(priv->display, "PENGUIN", False);
	priv->root_win = XCreateSimpleWindow(priv->display, 
		DefaultRootWindow(priv->display), -10, -10, 1, 1, 0, 0, 0);
	if(priv->root_win == -1) {
		log_error("XCreateSimpleWindow fail.");
		return NULL;
	}

	while(1)
	{
		XNextEvent(priv->display, &ev);
		switch (ev.type)
		{
			case SelectionClear:
				XConvertSelection(priv->display, priv->sel,
					XInternAtom(priv->display, "UTF8_STRING", False),
					property, priv->root_win, CurrentTime);
				printf("SelectionClear\n");
				break;
			case SelectionRequest:
				serqv = (XSelectionRequestEvent*)&ev.xselectionrequest;
				_on_selection_request(priv, serqv);
			case SelectionNotify:
				sev = (XSelectionEvent*)&ev.xselection;
				if (sev->property == None)
				{
					printf("Conversion could not be performed.\n");
				}else{
					show_utf8_prop(priv->display, priv->root_win, property);
				}
				break;
			case ClientMessage:
				goto exit;
				break;
		}
	}
exit:
	log_info("clip server exit.");
	return NULL;
}

static int xlib_dev_init(struct input_object *obj) {
	int ret = -1;
	struct xlib_input *priv;
	char * def_dpy = ":0.0";

	priv = calloc(1, sizeof(*priv));
	if(!priv) {
		log_error("calloc fail, check free memery.");
		goto FAIL1;
	}

	if(getenv("DISPLAY"))
		def_dpy=getenv("DISPLAY");

	if(XInitThreads() == 0)
	{
		log_warning("xlib not support multi-threaded");
	}
	priv->display = XOpenDisplay(def_dpy);

	if(priv->display == NULL) {
		log_error("XOpenDisplay: cannot connect to X server %s.",
				  XDisplayName(def_dpy));
		goto FAIL1;
	}

	pthread_create(&priv->clip_thread, NULL, _clip_server, (void *)priv);
	obj->priv = (void *)priv;
	return 0;
FAIL1:
	return ret;
}

static int xlib_set_info(struct input_object *obj, GHashTable *fb_info)
{
	return 0;
}

static long get_xkeycode(int keycode)
{
	switch(keycode)
	{
		case 1:
			return XK_Super_L;
		case 8:
			return XK_BackSpace;
		case 9:
			return XK_Tab;
		case 13:
			return XK_Return;
		case 16:
			return XK_Shift_L;
		case 17:
			return XK_Control_L;
		case 18:
			return XK_Alt_L;
		case 20:
			return XK_Caps_Lock;
		case 27:
			return XK_Escape;
		case 32:
			return XK_space;
		case 33:
			return XK_Page_Up;
		case 34:
			return XK_Page_Down;
		case 35:
			return XK_End;
		case 36:
			return XK_Home;
		case 37:
			return XK_Left;
		case 38:
			return XK_Up;
		case 39:
			return XK_Right;
		case 40:
			return XK_Down;
		case 46:
			return XK_Delete;
		case 108:
			return XK_Return;
		case 112:
			return XK_F1;
		case 113:
			return XK_F2;
		case 114:
			return XK_F3;
		case 115:
			return XK_F4;
		case 116:
			return XK_F5;
		case 117:
			return XK_F6;
		case 118:
			return XK_F7;
		case 119:
			return XK_F8;
		case 120:
			return XK_F9;
		case 121:
			return XK_F10;
		case 122:
			return XK_F11;
		case 123:
			return XK_F12;
		case 186:
			return XK_colon;
		case 187:
			return XK_equal;
		case 188:
			return XK_comma;
		case 189:
			return XK_minus;
		case 190:
			return XK_period;
		case 191:
			return XK_slash;
		case 220:
			return XK_backslash;
		case 219:
			return XK_bracketleft;
		case 221:
			return XK_bracketright;
		case 222:
			return XK_apostrophe;
	}

	if(keycode > 32 && keycode <= 200)
		return keycode;
	else
		return XK_Control_L;
}

extern float frame_scale;
static int xlib_push_key(struct input_object *obj, struct input_event *event)
{
	struct xlib_input *priv = (struct xlib_input *)obj->priv;

	switch (event->type) {
		case MOUSE_MOVE:
		{
			XTestFakeMotionEvent(priv->display, 0, event->x / frame_scale, event->y / frame_scale, 0);
			break;
		}
		case KEY_UP:
		{
			if (event->key_code == 0) return -1;
			XTestFakeKeyEvent(priv->display, XKeysymToKeycode(priv->display,get_xkeycode(event->key_code)), false, 0);
			break;
		}
		case KEY_DOWN:
		{
			if (event->key_code == 0) return -1;
			XTestFakeKeyEvent(priv->display, XKeysymToKeycode(priv->display,get_xkeycode(event->key_code)), true, 0);
			break;
		}
		case MOUSE_UP:
		{
			if (event->key_code == 0) return -1;
			XTestFakeButtonEvent(priv->display, event->key_code, false, 0);
			break;
		}
		case MOUSE_DOWN:
		{
			if (event->key_code == 0) return -1;
			XTestFakeButtonEvent(priv->display, event->key_code, true, 0);
			break;
		}
		case MOUSE_WHEEL:
		{
			if (event->key_code == 0) return -1;
			XTestFakeButtonEvent(priv->display, event->key_code, true, CurrentTime);
			XTestFakeButtonEvent(priv->display, event->key_code, false, CurrentTime);
			break;
		}
		default:
		{
			return -1;
		}
	}

	XFlush(priv->display);
	return 0;
}

static int xlib_push_clip(struct input_object *obj, struct clip_event *event)
{
	struct xlib_input *priv = (struct xlib_input *)obj->priv;
	uint16_t clip_len = ntohs(event->data_len);
	uint16_t type_len = 0;

	strcpy(priv->clip_type, (char *)event->clip_data);
	type_len = strlen(priv->clip_type) + 1;
	if(clip_len - type_len > 0)
	{
		memcpy(priv->clip_data, &event->clip_data[type_len], clip_len - type_len);
		priv->clip_len = clip_len - type_len;

		// 窗口A拥有剪贴板
		if(priv->root_win)
		{
			XSetSelectionOwner(priv->display, priv->sel, priv->root_win,
				CurrentTime);
		}
	}else{
		log_error("clip len to small.");
	}
	return 0;
}

static int xlib_dev_release(struct input_object *obj) {
	struct xlib_input *priv = (struct xlib_input *)obj->priv;
	XClientMessageEvent exit_event = {0};

	exit_event.type = ClientMessage;
	exit_event.window = priv->root_win;
	exit_event.format = 32;
	XSendEvent(priv->display, priv->root_win, 0, 0, (XEvent*)&exit_event);
	XFlush(priv->display);

	pthread_join(priv->clip_thread, NULL);

	XDestroyWindow(priv->display, priv->root_win);
	XCloseDisplay(priv->display);
	free(priv);
	return 0;
}

struct input_ops xlib_input_ops = {
	.name 				= "xlib_input",
	.init 				= xlib_dev_init,
	.set_info			= xlib_set_info,
	.push_key			= xlib_push_key,
	.push_clip 			= xlib_push_clip,
	.release 			= xlib_dev_release,
};

INPUT_ADD_DEV(99, xlib_input_ops);