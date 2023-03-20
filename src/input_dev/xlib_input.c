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
#include "input_event.h"

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
	    	// printf("%ld\n", XStringToKeysym("BackSpace"));
	switch(keycode)
	{
		case 8:
			return XStringToKeysym("BackSpace");
		case 9:
			return XStringToKeysym("Tab");
		case 13:
			return XStringToKeysym("Return");
		case 16:
			return XStringToKeysym("Shift_L");
		case 17:
			return XStringToKeysym("Control_L");
		case 18:
			return XStringToKeysym("Alt_L");
		case 20:
			return XStringToKeysym("CapsLock");
		case 27:
			return XStringToKeysym("Escape");
		case 32:
			return XStringToKeysym("space");
		case 33:
			return XStringToKeysym("PageUp");
		case 34:
			return XStringToKeysym("PageDown");
		case 35:
			return XStringToKeysym("End");
		case 36:
			return XStringToKeysym("Home");
		case 37:
			return XStringToKeysym("Left");
		case 38:
			return XStringToKeysym("Up");
		case 39:
			return XStringToKeysym("Right");
		case 40:
			return XStringToKeysym("Down");
	}

	if(keycode > 32 && keycode <= 222)
		return keycode;
	else
		return XStringToKeysym("Control_L");
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

struct input_dev_ops xlib_input_dev_ops = {
	.name 				= "xlib_input_dev",
	.init 				= xlib_dev_init,
	.set_info			= xlib_set_info,
	.push_key			= xlib_push_key,
	.release 			= xlib_dev_release,
};