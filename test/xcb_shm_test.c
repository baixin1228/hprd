#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/shm.h>
#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <xcb/xproto.h>


static int check_shm(xcb_connection_t *conn)
{
	xcb_shm_query_version_cookie_t cookie = xcb_shm_query_version(conn);
	xcb_shm_query_version_reply_t *reply;

	reply = xcb_shm_query_version_reply(conn, cookie, NULL);
	if (reply) {
		free(reply);
		return 1;
	}

	return 0;
}

static int xcbgrab_frame_shm(xcb_connection_t *conn, xcb_screen_t *screen, int width, int height)
{
	xcb_shm_get_image_cookie_t iq;
	xcb_shm_get_image_reply_t *img;
	xcb_generic_error_t *e = NULL;

	int ret;
	xcb_shm_seg_t segment;
	uint32_t shmid;
	uint8_t *data;

	shmid = shmget(IPC_PRIVATE, width * height * 32, IPC_CREAT | 0777);
	data = (uint8_t*)(shmat(shmid, 0, 0));
	segment = (xcb_shm_seg_t)xcb_generate_id(conn);
	xcb_shm_attach(conn, segment, shmid, 0);
	xcb_flush(conn);

	for (int i = 0; i < 1000; ++i)
	{
		iq = xcb_shm_get_image(conn, screen->root,
							   0, 0, width, height, ~0,
							   XCB_IMAGE_FORMAT_Z_PIXMAP, segment, 0);
		img = xcb_shm_get_image_reply(conn, iq, &e);

		xcb_flush(conn);

		if (e) {
			printf("Cannot get the image data event_error: response_type:%u error_code:%u sequence:%u resource_id:%u minor_code:%u major_code:%u.\n",
				   e->response_type, e->error_code,
				   e->sequence, e->resource_id, e->minor_code, e->major_code);
			free(e);
			ret = -1;
			goto FAIL1;
		}
		// printf("xcb shm get image idx:%d data:%x\n", i, data[0]);
		// usleep(16666);
		free(img);
	}

FAIL1:
	xcb_shm_detach(conn, segment);
	shmdt(data);
	ret = shmctl(shmid, IPC_RMID, 0);
	assert(ret != -1);
	return ret;
}

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

	printf("xcb shm informations of screen:%d width:%d height:%d\n" , screen_num , screen->width_in_pixels , screen->height_in_pixels);
	m_iDesktopWidth = screen->width_in_pixels;
	m_iDesktopHeight = screen->height_in_pixels;

	if(check_shm(conn))
	{
		xcbgrab_frame_shm(conn, screen, m_iDesktopWidth, m_iDesktopHeight);
	}

	return 0;
}