#include <stdio.h>
#include <malloc.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <wayland-client.h>
#include <libdrm/drm_fourcc.h>
#include <wayland-egl-core.h>
#include <stdbool.h>
#include <sys/time.h>
#include <pthread.h>
#include <dlfcn.h>
#include "display_engine.h"
#include "gl/gl_help.h"
#include "util.h"

EGLint config_attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE
};
EGLint context_attribs[] = {
	EGL_CONTEXT_CLIENT_VERSION, 2,
	EGL_NONE, EGL_NONE
};

EGLConfig egl_config;

bool __create_wl_win(struct display_engine *display, void *wl_surface)
{
	struct wl_egl_window *egl_window;

	if(wl_surface == NULL)
		return false;

	egl_window = wl_egl_window_create(wl_surface, display->width, display->height);
	if(egl_window > 0x30000000)
		log_error("egl_window:%p\n", egl_window);
	else
		log_warning("egl_window:%p\n", egl_window);
	display->eglWindow = egl_window;

	return true;
}

bool __create_egl_surf(struct display_engine *display)
{
	EGLSurface egl_surface;

	egl_surface = eglCreateWindowSurface(
			display->eglDisplay,
			egl_config,
			display->eglWindow,
			0);
	if(!egl_surface)
	{
		line_error("eglCreateWindowSurface fail.\n");
		line_error("一般是egl_window初始化有问题，请联系作者。\n");
		return false;
	}
	display->eglSurface = egl_surface;

	return true;
}

bool wl_init_egl(struct display_engine *display, void * native_display, void * wl_surface)
{
	int i;
	EGLBoolean ret;
	EGLContext egl_context;
	// init egl
	EGLint major, minor, config_count, tmp_n;
	static bool is_init_egl_dispaly = false;

	if(!is_init_egl_dispaly)
	{
		load_egl_api(NULL);

		ret = eglInitialize(native_display, &major, &minor);
		if(ret == EGL_FALSE)
		{
			line_error("eglInitialize fail.\n");
			line_error("一般是环境有问题wayland没有安装好。\n");
			return false;
		}

		load_egl_api(native_display);
		log_info("EGL major: %d, minor %d\n", major, minor);

		ret = eglGetConfigs(native_display, 0, 0, &config_count);
		if(ret == EGL_FALSE)
		{
			line_error("glGetConfigs fail.\n");
			line_error("一般是显卡驱动安装有问题。\n");
			return false;
		}
		ret = eglChooseConfig(native_display, config_attribs, &(egl_config), 1, &tmp_n);
		if(ret == EGL_FALSE)
		{
			line_error("glChooseConfig fail.\n");
			line_error("一般是显卡驱动安装有问题。\n");
			return false;
		}
	}

	egl_context = eglCreateContext(
			native_display,
			egl_config,
			EGL_NO_CONTEXT,
			context_attribs);
	if(!egl_context)
	{
		line_error("eglCreateContext fail.\n");
		line_error("一般是显卡驱动安装有问题。\n");
		return false;
	}
	display->eglContext = egl_context;
	display->eglDisplay = native_display;

	if(wl_surface)
	{
		if(!__create_wl_win(display, wl_surface))
		{
			return false;
		}
	}else{
		line_error("wl_surface is null!\n");
	}


	if(!__create_egl_surf(display))
		return false;

	make_current(display);
	if(!is_init_egl_dispaly)
	{
		/* 关闭帧同步 */
		if (!eglSwapInterval(native_display, 1)) {
			line_warning("** Failed to set swap interval. Results may be bounded above by refresh rate.\n");
		}
		load_eges2_api();
	}

	glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	eglSwapBuffers(display->eglDisplay, display->eglSurface);

	is_init_egl_dispaly = true;
	return true;
}

bool wl_show_surface(struct display_engine *display, void *wl_surface)
{
	if(wl_surface == NULL)
		return false;

	if(!__create_wl_win(display, wl_surface))
		return false;

	return __create_egl_surf(display);
}

bool wl_hide_surface(struct display_engine *display)
{
	EGLBoolean ret;

	if(display->eglSurface)
	{
		ret = eglDestroySurface(display->eglDisplay, display->eglSurface);
		if(!ret)
		{
			line_error("eglDestroySurface fail.\n");
			return ret;
		}
		display->eglSurface = NULL;
	}

	if(display->eglWindow)
	{
		wl_egl_window_destroy(display->eglWindow);
		display->eglWindow = NULL;
	}

	ret = eglReleaseThread();
	if(!ret)
	{
		line_error("eglReleaseThread fail.\n");
		return ret;
	}
	return true;
}

void wl_change_size(struct display_engine *display, void *wl_surface, void *compositor)
{
    struct wl_region *region;
	wl_egl_window_resize(display->eglWindow, display->width, display->height, 0, 0);
    region = wl_compositor_create_region(compositor);
    wl_region_add(region, 0, 0, display->width, display->height);
    wl_surface_set_opaque_region(wl_surface, region);
    wl_region_destroy(region);
}

bool wl_release_egl(struct display_engine *display)
{
	EGLBoolean ret;
	ret = eglDestroyContext(display->eglDisplay, display->eglContext);
	if(!ret)
	{
		line_error("eglDestroyContext fail.\n");
		return ret;
	}
	display->eglContext = NULL;

	if(display->eglSurface)
	{
		ret = eglDestroySurface(display->eglDisplay, display->eglSurface);
		if(!ret)
		{
			line_error("eglDestroySurface fail.\n");
			return ret;
		}
		display->eglSurface = NULL;
	}

	if(display->eglWindow)
	{
		wl_egl_window_destroy(display->eglWindow);
		display->eglWindow = NULL;
	}
	
	display->eglDisplay = NULL;

	ret = eglReleaseThread();
	if(!ret)
	{
		line_error("eglReleaseThread fail.\n");
		return ret;
	}
	return true;
}