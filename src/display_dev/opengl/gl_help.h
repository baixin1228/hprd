#ifndef __GL_HELP_H__
#define __GL_HELP_H__

#include "GL/glew.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "common.h"

struct gl_texture
{
	GLuint	texture;
	EGLImageKHR image;
	GLuint	Y_texture;
	EGLImageKHR Y_image;
	GLuint	U_texture;
	EGLImageKHR U_image;
	GLuint	V_texture;
	EGLImageKHR V_image;
	GLuint	UV_texture;
	EGLImageKHR UV_image;
};

#define SURFACE_BUFFER_COUNT 25
struct gl_object
{
	GLuint	shader;	// Handle to a program object
	GLint	positions;	// Attribute locations
	GLint	tex_coord;
	GLint	samplerLoc;
	GLint	y_sampler;
	GLint	u_sampler;
	GLint	v_sampler;
	GLint	uv_sampler;
	GLint	mvp;		// Uniform locations
	GLfloat	mvp_matrix[4][4]; // MVP matrix
	GLint	width;
	GLint	height;
	struct gl_texture texture[SURFACE_BUFFER_COUNT];
	struct shader_render_ops *gl_ops;
};

struct shader_render_ops {
	int (* init)(struct gl_object *obj);
	int (* render)(struct gl_object *obj, struct gl_texture *texture);
	int (* release)(struct gl_object *obj);
};

void checkGlError(char *file, unsigned int  LineNumber);
GLuint gl_load_program(const char *vertShaderSrc, const char *fragShaderSrc);
struct gl_object *gl_init(uint32_t width, uint32_t height, int pix_format);
int gl_render(struct gl_object *obj, int texture_id);
void gl_release(struct gl_object *obj);
void gl_show_version();

#endif