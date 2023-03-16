#ifndef __GL_RENDER_H__
#define __GL_RENDER_H__

#include <GLES2/gl2.h>
#include <GLES2/gl2platform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common.h"
#include "gl_help.h"

struct shader_render_ops {
	int (* init)(struct gl_object *obj);
	int (* bind_pbo)(struct gl_texture *texture, char *data[4]);
	int (* transport_pbo)(struct gl_texture *texture, char *data[4]);
	int (* bind_khr)(struct gl_texture *texture, char *data[4]);
	int (* render)(struct gl_object *obj, struct gl_texture *texture);
	int (* release)(struct gl_object *obj);
};

struct gl_object *gl_init(uint32_t width, uint32_t height, int pix_format);
int gl_bind_pbo(struct gl_object *obj, struct raw_buffer *buffer);
int gl_transport_pbo(struct gl_object *obj, struct raw_buffer *buffer);
int gl_bind_khr(struct gl_object *obj, struct gl_texture *texture,
	struct raw_buffer *buffer);
int gl_render(struct gl_object *obj, int texture_id);
void gl_release(struct gl_object *obj);
void gl_show_version();
#endif