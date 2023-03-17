#ifndef __GL_HELP_H__
#define __GL_HELP_H__

#include <GL/gl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "common.h"
#include "frame_buffer.h"

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
	uint32_t width;
	uint32_t height;
	uint32_t h_stride;
	uint32_t v_stride;
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

void checkEGlError(char *file, unsigned int  LineNumber);
void checkGlError(char *file, unsigned int  LineNumber);
GLuint gl_load_program(const char *vertShaderSrc, const char *fragShaderSrc);
void gl_create_pbo_texture(GLuint *gl_texture, uint32_t h_stride,
	uint32_t width, uint32_t height, char *data);
void gl_update_pbo_texture(GLuint *gl_texture, uint32_t h_stride,
	uint32_t width, uint32_t height, char *data);
#endif