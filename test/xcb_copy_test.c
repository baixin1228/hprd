#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

xcb_get_image_reply_t *img;
xcb_get_image_cookie_t iq;
xcb_generic_error_t *e = NULL;
xcb_connection_t *conn;
xcb_screen_t *screen;
int m_iDesktopWidth = 1280;
int m_iDesktopHeight = 1024;

int main()
{
	int screen_num = 1;

	conn = xcb_connect(NULL, &screen_num);

	if (xcb_connection_has_error(conn))
	{
		printf("xcb_connection_has_error faild!");
		return -1;
	}

	const xcb_setup_t * setup = xcb_get_setup(conn);
	xcb_screen_iterator_t iter = xcb_setup_roots_iterator (setup);

	for (int i = 0; i < screen_num; ++i)
	{
		xcb_screen_next(&iter);
	}

	screen = iter.data;

	printf("xcb copy informations of screen:%d width:%d height:%d\n" , screen_num , screen->width_in_pixels , screen->height_in_pixels);
	m_iDesktopWidth = screen->width_in_pixels;
	m_iDesktopHeight = screen->height_in_pixels;

	for (int i = 0; i < 1000; ++i)
	{
		//xgrab方法
		iq = xcb_get_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, screen->root, 0, 0, m_iDesktopWidth, m_iDesktopHeight, ~0);
		img = xcb_get_image_reply(conn, iq, &e);
		if (e)
		{
			printf("xcb_get_image_reply faild!");
			continue;
		}
		// uint8_t *data;
		if (!img)
		{
			printf("img = NULL faild!");
			continue;
		}
		// data   = xcb_get_image_data(img);
		// int length = xcb_get_image_data_length(img);
		// printf("xcb copy get image idx:%d data:%x\n", i, data[0]);
		// usleep(16666);
		free(img);
	}
	return 0;
}