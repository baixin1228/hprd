#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "util.h"
#include "allocator.h"

static int _open_drm()
{
	static int _drm_fd = -1;

	if(_drm_fd == -1)
		_drm_fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);

	return _drm_fd;
}

struct drm_buffer *create_framebuffer(uint32_t width, uint32_t height, uint16_t bpp)
{
	struct drm_mode_create_dumb create = { 0 };
 	struct drm_mode_map_dumb map = { 0 };
 	struct drm_buffer *fb;
 	int fd, ret;

 	if(bpp != 16 && bpp != 24 && bpp != 32)
 	{
 		log_error("bpp is invalid.");
 		return NULL;
 	}

 	fd = _open_drm();
 	if(fd == -1)
 	{
 		log_error("open drm fail.");
 		return NULL;
 	}

 	fb = calloc(1, sizeof(*fb));
 	if(fb == NULL)
 	{
 		log_error("calloc fail.");
 		return NULL;
 	}

	create.width = width;
	create.height = height;
	create.bpp = bpp;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	if(ret)
	{
 		log_error("drm create dumb fail.");
 		goto fail;
	}

	fb->pitch = create.pitch;
	fb->size = create.size;
	fb->handle = create.handle;

	map.handle = create.handle;
	ret = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
	if(ret)
	{
 		log_error("drm map dumb fail.");
 		goto fail;
	}

	fb->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
			MAP_SHARED, fd, map.offset);
	if(fb->vaddr == NULL)
	{
 		log_error("drm mmap fail.");
 		goto fail;
	}

	memset(fb->vaddr, 0xff, fb->size);

	return fb;

fail:
 	free(fb);
 	return NULL;
}

void release_framebuffer(struct drm_buffer *fb)
{
 	int fd;
	struct drm_mode_destroy_dumb destroy = { 0 };

	munmap(fb->vaddr, fb->size);

 	fd = _open_drm();
 	if(fd != -1)
 	{
 		destroy.handle = fb->handle;
		drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
 	} else {
 		log_error("open drm fail.");
 	}

	free(fb);
}