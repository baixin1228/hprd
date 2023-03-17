#include "util.h"
#include "gl_help.h"
#include "gl_render.h"

int yuv420_init(struct gl_object *obj)
{
	GLbyte vShaderStr[] =
		"#version 100                                \n"
		"uniform mat4 u_mvp_matrix;                   \n"
		"attribute vec4 a_position;                  \n"
		"attribute vec2 a_texCoord;                  \n"
		"varying vec2 v_texCoord;                    \n"
		"void main()                                 \n"
		"{                                           \n"
		"   gl_Position = u_mvp_matrix * a_position;  \n"
		"   v_texCoord = a_texCoord;                 \n"
		"}                                           \n";

	GLbyte fShaderStr[] =
		"#version 100                                        \n"
		"precision mediump float;                            \n"
		"varying vec2 v_texCoord;                            \n"
		"uniform sampler2D y_texture;                        \n"
		"uniform sampler2D u_texture;                        \n"
		"uniform sampler2D v_texture;                        \n"
		"void main()                                         \n"
		"{                                                   \n"
		"  float y,u,v;                                      \n"
		"  float sub = 16.0 / 256.0;                         \n"

		"  y = texture2D(y_texture, v_texCoord).r;           \n"
		"  u = texture2D(u_texture, v_texCoord).r;			 \n"
		"  v = texture2D(v_texture, v_texCoord).r; 			 \n"

		/* BT709 标准*/
		"  gl_FragColor.b = 1.164 * (y - sub) + 2.115 * (u - 0.5);\n"
		"  gl_FragColor.g = 1.164 * (y - sub) - 0.534 * (v - 0.5) - 0.213 * (u - 0.5);\n"
		"  gl_FragColor.r = 1.164 * (y - sub) + 1.793 * (v - 0.5);\n"
		"  gl_FragColor.a = 1.0;\n"
		"}                                                   \n";

	// Load the shaders and get a linked program object
	obj->shader = gl_load_program((const char*)vShaderStr, (const char*)fShaderStr);
	checkEGlError(__FILE__, __LINE__);

	// Get the attribute locations
	obj->positions = glGetAttribLocation(obj->shader, "a_position");
	checkEGlError(__FILE__, __LINE__);
	obj->tex_coord = glGetAttribLocation(obj->shader, "a_texCoord");
	checkEGlError(__FILE__, __LINE__);

	// Get the uniform locations
	obj->mvp = glGetUniformLocation(obj->shader, "u_mvp_matrix");
	checkEGlError(__FILE__, __LINE__);

	obj->y_sampler = glGetUniformLocation(obj->shader, "y_texture");
	checkEGlError(__FILE__, __LINE__);
	obj->u_sampler = glGetUniformLocation(obj->shader, "u_texture");
	checkEGlError(__FILE__, __LINE__);
	obj->v_sampler = glGetUniformLocation(obj->shader, "v_texture");
	checkEGlError(__FILE__, __LINE__);

	return 0;
}

int yuv420_bind_pbo(struct gl_texture *texture, char *data[4])
{
	uint32_t height;
	height = texture->v_stride > texture->height ? texture->v_stride : texture->height; 

	gl_create_pbo_texture(&texture->Y_texture, texture->h_stride, texture->width ,height,
		data[0]);
	gl_create_pbo_texture(&texture->U_texture, texture->h_stride / 2,
		texture->width / 2, height / 2, data[1]);
	gl_create_pbo_texture(&texture->V_texture, texture->h_stride / 2,
		texture->width / 2, height / 2, data[2]);
	return 0;
}

int yuv420_transport_pbo(struct gl_texture *texture, char *data[4])
{
	uint32_t height;
	height = texture->v_stride > texture->height ? texture->v_stride : texture->height; 

	gl_update_pbo_texture(&texture->Y_texture, texture->h_stride, texture->width, height,
		data[0]);
	gl_update_pbo_texture(&texture->U_texture, texture->h_stride / 2,
		texture->width / 2, height / 2, data[1]);
	gl_update_pbo_texture(&texture->V_texture, texture->h_stride / 2,
		texture->width / 2, height / 2, data[2]);
	return 0;
}

int yuv420_bind_khr(struct gl_texture *texture, char *data[4])
{
	return 0;
}

int yuv420_render(struct gl_object *obj, struct gl_texture *texture)
{
	glActiveTexture(GL_TEXTURE0);
	checkEGlError(__FILE__, __LINE__);
	glBindTexture(GL_TEXTURE_2D, texture->Y_texture);
	checkEGlError(__FILE__, __LINE__);
	glUniform1i (obj->y_sampler, 0);
	checkEGlError(__FILE__, __LINE__);

	glActiveTexture(GL_TEXTURE1);
	checkEGlError(__FILE__, __LINE__);
	glBindTexture(GL_TEXTURE_2D, texture->U_texture);
	checkEGlError(__FILE__, __LINE__);
	glUniform1i (obj->u_sampler, 1);
	checkEGlError(__FILE__, __LINE__);

	glActiveTexture(GL_TEXTURE2);
	checkEGlError(__FILE__, __LINE__);
	glBindTexture(GL_TEXTURE_2D, texture->V_texture);
	checkEGlError(__FILE__, __LINE__);
	glUniform1i (obj->v_sampler, 2);
	checkEGlError(__FILE__, __LINE__);
	return 0;
}

int yuv420_release(struct gl_object *obj)
{
	if(obj->shader)
	{
		glDeleteProgram(obj->shader);
		obj->shader = 0;
	}
	return 0;
}

struct shader_render_ops yuv420_ops = 
{
	.init 			= yuv420_init,
	.bind_pbo 		= yuv420_bind_pbo,
	.transport_pbo 	= yuv420_transport_pbo,
	.bind_khr 		= yuv420_bind_khr,
	.render 		= yuv420_render,
	.release		= yuv420_release,
};