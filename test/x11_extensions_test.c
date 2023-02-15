#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

static int check_shm(Display* display)
{
	if (XShmQueryExtension(display) == True)
		return 1;

	return 0;
}

static int ex_frame_shm(Display* display)
{
	int ret = -1;
	int screen_num = 1;
	Window win;
	XWindowAttributes windowattr;
    Visual* visual;
    int windowdepth;
	XShmSegmentInfo shm;
	XImage *xim;

	shm.shmid = -1;
	shm.shmaddr = (char *) -1;

	screen_num = DefaultScreen(display);

	win = RootWindow(display, screen_num);
    int w = DisplayWidth(display, screen_num);
    int h = DisplayHeight(display, screen_num);
	printf("x11 xext informations of screen:%d width:%d height:%d\n" , screen_num , w , h);

    if(XGetWindowAttributes(display, win,
        &windowattr) == 0)
    {
        printf("icvVideoRender: failed to get window attributes.\n");
        return -1;
    }
    windowdepth = windowattr.depth;
    visual      = windowattr.visual;

	xim = XShmCreateImage(display, visual, windowdepth, ZPixmap, NULL,
	    &shm, w, h);

	if (xim == NULL) {
		printf("XShmCreateImage failed.\n");
		goto FAIL1;
	}

	shm.shmid = shmget(IPC_PRIVATE,
	    (size_t)xim->bytes_per_line * xim->height, IPC_CREAT | 0600);

	if (shm.shmid == -1) {
		printf("shmget failed.\n");
		goto FAIL2;
	}

	shm.readOnly = False;
	shm.shmaddr = xim->data = (char *) shmat(shm.shmid, 0, 0);

	if (shm.shmaddr == (char *)-1) {
		printf("shmat failed.\n");
		goto FAIL3;
	}

	if (!XShmAttach(display, &shm)) {
		printf("XShmAttach failed.\n");
		goto FAIL4;
	}
	XSync(display, False);

	for (int i = 0; i < 1000; ++i)
	{
		XShmGetImage(display, win, xim,
			     0, 0, AllPlanes);
		XSync(display, False);
		// printf("x11 xext get image idx:%d data:%x\n", i, xim->data[0]);
	}

	ret = 0;

	XShmDetach(display, &shm);
FAIL4:
	shmdt(shm.shmaddr);
FAIL3:
	shmctl(shm.shmid, IPC_RMID, 0);
FAIL2:
	XDestroyImage(xim);
FAIL1:
	return ret;
}

#define DISPLAY_NAME ":0"
int main()
{
	Display* display;

	display=XOpenDisplay(DISPLAY_NAME);
    
    if(display == NULL)
    {
        fprintf( stderr, "cvVideo: cannot displayect to X server %s\n",
            XDisplayName(DISPLAY_NAME));
        return -1;
    }
	if(check_shm(display))
	{
		ex_frame_shm(display);
	}

	printf("\n");
	return 0;
}