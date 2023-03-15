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

#define S1 1.0f
#define S2 1.0f
#define pA - S1,   S2, -1,
#define pB   S1,   S2, -1,
#define pC - S1, - S2, -1,
#define pD   S1, - S2, -1,

GLfloat g_plane_verts[] =
{
	pA pB
	pC pD
};

GLfloat v_tex_vertices[] =
{
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 1.0f, 1.0f,
};

struct gl_object *gl_init(uint32_t width, uint32_t height, int pix_format)
{
	struct gl_object* obj = calloc(1, sizeof(struct gl_object));

	obj->width = width == 0 ? 640 : width;
	obj->height = height == 0 ? 480 : height;

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			if(i == j)
				obj->mvp_matrix[i][j] = 1;
			else
				obj->mvp_matrix[i][j] = 0;
		}
	}
	
	switch(pix_format)
	{
		case ARGB8888:
		{
			break;
		}
		case YUV420P:
		{
			break;
		}
		case NV12:
		{
			extern struct shader_render_ops nv12_ops;
			obj->gl_ops = &nv12_ops;
			break;
		}
		default:
		{
			break;
		}
	}

	if(obj->gl_ops == NULL)
		goto FAIL;

	if(obj->gl_ops->init(obj) == -1)
		goto FAIL;

	glUseProgram(obj->shader);
	checkGlError(__FILE__, __LINE__);

	glEnableVertexAttribArray(obj->positions);
	checkGlError(__FILE__, __LINE__);
	glVertexAttribPointer(obj->positions, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), g_plane_verts);
	checkGlError(__FILE__, __LINE__);
	glUniformMatrix4fv(obj->mvp, 1, GL_FALSE, (GLfloat*) obj->mvp_matrix);
	checkGlError(__FILE__, __LINE__);
	glEnableVertexAttribArray (obj->tex_coord);
	checkGlError(__FILE__, __LINE__);
	glVertexAttribPointer(obj->tex_coord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), v_tex_vertices);
	checkGlError(__FILE__, __LINE__);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	checkGlError(__FILE__, __LINE__);
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	checkGlError(__FILE__, __LINE__);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	checkGlError(__FILE__, __LINE__);
	// Set the viewport
	glViewport(0, 0, obj->width, obj->height);
	checkGlError(__FILE__, __LINE__);
	glCullFace(GL_BACK);
	checkGlError(__FILE__, __LINE__);
	glEnable(GL_CULL_FACE);
	checkGlError(__FILE__, __LINE__);
	glEnable(GL_DEPTH_TEST);
	checkGlError(__FILE__, __LINE__);
	glDepthFunc(GL_LESS);
	checkGlError(__FILE__, __LINE__);

	return obj;
FAIL:
	free(obj);
	return NULL;
}

int gl_render(struct gl_object *obj, int texture_id)
{
	return obj->gl_ops->render(obj, &obj->texture[texture_id]);
}

void gl_release(struct gl_object *obj)
{
	obj->gl_ops->release(obj);
	free(obj);
}

void gl_show_version()
{
	log_info("OpenGL实现厂商的名字：%s\n", glGetString(GL_VENDOR));
	log_info("渲染器标识符：%s\n", glGetString(GL_RENDERER));
	log_info("OOpenGL实现的版本号：%s\n", glGetString(GL_VERSION));
}