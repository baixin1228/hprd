#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dlfcn.h>
#include <libdrm/drm_fourcc.h>

#include "util.h"
#include "gl_help.h"
#include "frame_buffer.h"

void checkGlError(char *file, unsigned int  LineNumber)
{
	EGLint error = glGetError();
	if (error != EGL_SUCCESS)
	{
		log_error("eglGetError: %04X at file:%s line:%d\n",error, file, LineNumber);
		exit(-1);
	}
}

void checkEGlError(char *file, unsigned int  LineNumber)
{
	EGLint error = eglGetError();
	if (error != EGL_SUCCESS)
	{
		log_error("eglGetError: %04X at file:%s line:%d\n",error, file, LineNumber);
		exit(-1);
	}
}

GLuint __gl_load_shader(GLenum type, const char *shaderSrc)
{
	GLuint shader;
	GLint compiled;

	// Create the shader object
	shader = glCreateShader(type);

	if(shader == 0)
		return 0;

	// Load the shader source
	glShaderSource(shader, 1, &shaderSrc, NULL);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if(!compiled)
	{
		GLint infoLen = 0;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
		if(infoLen > 1)
		{
			char* infoLog = (char*)malloc (sizeof(char) * infoLen);
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			log_error("Error compiling shader:\n%s", infoLog);
			free(infoLog);
			exit(-1);
		}

		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

GLuint gl_load_program(const char *vertShaderSrc, const char *fragShaderSrc)
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint shader;
	GLint linked;

	// Load the vertex/fragment shaders
	vertexShader = __gl_load_shader(GL_VERTEX_SHADER, vertShaderSrc);
	if(vertexShader == 0)
		return 0;

	fragmentShader = __gl_load_shader(GL_FRAGMENT_SHADER, fragShaderSrc);
	if(fragmentShader == 0)
	{
		glDeleteShader(vertexShader);
		return 0;
	}

	// Create the program object
	shader = glCreateProgram();

	if(shader == 0)
		return 0;

	glAttachShader(shader, vertexShader);
	glAttachShader(shader, fragmentShader);

	// Link the program
	glLinkProgram(shader);

	// Check the link status
	glGetProgramiv(shader, GL_LINK_STATUS, &linked);

	if(!linked)
	{
		GLint infoLen = 0;

		glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

		if(infoLen > 1)
		{
			char* infoLog = (char*)malloc (sizeof(char) * infoLen);

			glGetProgramInfoLog(shader, infoLen, NULL, infoLog);
			log_error("Error linking program:\n%s", infoLog);
			free(infoLog);
		}

		glDeleteProgram(shader);
		return 0;
	}

	// Free up no longer needed shader resources
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	return shader;
}

void gl_create_pbo_texture(GLuint *gl_texture, uint32_t width, uint32_t height,
	char *data)
{
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, gl_texture);
    glBindTexture(GL_TEXTURE_2D, *gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 width, height,
                 0, GL_RED, GL_UNSIGNED_BYTE,
                 data);
}

void gl_update_pbo_texture(GLuint *gl_texture, uint32_t width, uint32_t height,
	char *data)
{
    glBindTexture(GL_TEXTURE_2D, *gl_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                 width, height,
                 GL_RED, GL_UNSIGNED_BYTE,
                 data);
}