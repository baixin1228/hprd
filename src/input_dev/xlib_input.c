#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>

#include <glib.h>

#include "util.h"
#include "input_dev.h"
#include "protocol.h"

#define DEFAULT_DISPLAY ":0"

struct xlib_input {
	Display *display;
	uint32_t screen_num;
	Window root_win;
};

static int xlib_dev_init(struct input_object *obj) {
	int ret = -1;
	struct xlib_input *priv;

	priv = calloc(1, sizeof(*priv));
	if(!priv) {
		log_error("calloc fail, check free memery.");
		goto FAIL1;
	}

	priv->display = XOpenDisplay(DEFAULT_DISPLAY);

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

	if(keycode > 32 && keycode <= 222)
		return keycode;
	else
		return XK_Control_L;
}

static int xlib_push_key(struct input_object *obj, struct input_event *event)
{
	struct xlib_input *priv = (struct xlib_input *)obj->priv;

    switch (event->type) {
	    case MOUSE_MOVE:
	    {
	        XTestFakeMotionEvent(priv->display, 0, event->x, event->y, 0);
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

static int xlib_dev_release(struct input_object *obj) {
	struct xlib_input *priv = (struct xlib_input *)obj->priv;

	XCloseDisplay(priv->display);
	free(priv);
	return 0;
}

struct input_ops xlib_input_ops = {
	.name 				= "xlib_input",
	.init 				= xlib_dev_init,
	.set_info			= xlib_set_info,
	.push_key			= xlib_push_key,
	.release 			= xlib_dev_release,
};

INPUT_ADD_DEV(99, xlib_input_ops);