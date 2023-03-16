#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dlfcn.h>
#include <libdrm/drm_fourcc.h>

#include "util.h"
#include "gl_help.h"
#include "gl_render.h"
#include "frame_buffer.h"

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
			extern struct shader_render_ops yuv420_ops;
			obj->gl_ops = &yuv420_ops;
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
	{
		log_error("gl_init unknow pix format.");
		goto FAIL;
	}

	if(obj->gl_ops->init(obj) == -1)
		goto FAIL;

	glUseProgram(obj->shader);
	checkEGlError(__FILE__, __LINE__);

	glEnableVertexAttribArray(obj->positions);
	checkEGlError(__FILE__, __LINE__);
	glVertexAttribPointer(obj->positions, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), g_plane_verts);
	checkEGlError(__FILE__, __LINE__);
	glUniformMatrix4fv(obj->mvp, 1, GL_FALSE, (GLfloat*) obj->mvp_matrix);
	checkEGlError(__FILE__, __LINE__);
	glEnableVertexAttribArray (obj->tex_coord);
	checkEGlError(__FILE__, __LINE__);
	glVertexAttribPointer(obj->tex_coord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), v_tex_vertices);
	checkEGlError(__FILE__, __LINE__);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// glViewport(0, 0, obj->width, obj->height);
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// glCullFace(GL_BACK);
	// checkEGlError(__FILE__, __LINE__);
	// glEnable(GL_CULL_FACE);
	// checkEGlError(__FILE__, __LINE__);
	// glEnable(GL_DEPTH_TEST);
	// checkEGlError(__FILE__, __LINE__);
	// glDepthFunc(GL_LESS);
	// checkEGlError(__FILE__, __LINE__);

	return obj;
FAIL:
	free(obj);
	return NULL;
}

int gl_bind_pbo(struct gl_object *obj, struct raw_buffer *buffer)
{
	obj->texture[0].width = buffer->width;
	obj->texture[0].height = buffer->height;
	obj->texture[0].h_stride = buffer->hor_stride;
	obj->texture[0].v_stride = buffer->ver_stride;
	return obj->gl_ops->bind_pbo(&obj->texture[0], buffer->ptrs);
}

int gl_transport_pbo(struct gl_object *obj, struct raw_buffer *buffer)
{
	obj->texture[0].width = buffer->width;
	obj->texture[0].height = buffer->height;
	obj->texture[0].h_stride = buffer->hor_stride;
	obj->texture[0].v_stride = buffer->ver_stride;
	return obj->gl_ops->transport_pbo(&obj->texture[0], buffer->ptrs);
}

int gl_bind_khr(struct gl_object *obj, struct gl_texture *texture,
	struct raw_buffer *buffer)
{
	return obj->gl_ops->bind_khr(texture, buffer->ptrs);
}

/* Triangular order */
GLuint rect_points[] =
{
	0, 3, 1,
	0, 2, 3,
};
int gl_render(struct gl_object *obj, int texture_id)
{
	if(obj->gl_ops->render(obj, &obj->texture[texture_id]) == -1)
		return -1;

	glDrawElements (GL_TRIANGLES, 6 , GL_UNSIGNED_INT, rect_points);
	checkEGlError(__FILE__, __LINE__);
	return 0;
}

void gl_release(struct gl_object *obj)
{
	obj->gl_ops->release(obj);
	free(obj);
}

void gl_show_version()
{
	log_info("OpenGL实现厂商的名字：%s", glGetString(GL_VENDOR));
	log_info("渲染器标识符：%s", glGetString(GL_RENDERER));
	log_info("OOpenGL实现的版本号：%s", glGetString(GL_VERSION));
}