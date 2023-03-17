#include "util.h"
#include "gl_help.h"
#include "gl_render.h"

int nv12_init(struct gl_object *obj)
{
	log_info("nv12");
	GLbyte v_shader_str[] =
		"#version 100                                \n"
		"uniform mat4 u_mvp_matrix;                   \n"
		"attribute vec4 a_position;                  \n"
		"attribute vec2 a_tex_coord;                  \n"
		"varying vec2 v_tex_coord;                    \n"
		"void main()                                 \n"
		"{                                           \n"
		"   gl_Position = u_mvp_matrix * a_position;  \n"
		"   v_tex_coord = a_tex_coord;                 \n"
		"}                                           \n";

	GLbyte f_shader_str[] =
		"#version 100                                        \n"
		"precision mediump float;                            \n"
		"varying vec2 v_tex_coord;                            \n"
		"uniform sampler2D y_texture;                        \n"
		"uniform sampler2D uv_texture;                        \n"
		"void main()                                         \n"
		"{                                                   \n"
		"  float y,u,v;                                      \n"
		"  float sub = 16.0 / 256.0;                         \n"
		"  y = texture2D(y_texture, v_tex_coord).r;           \n"
		/* full range 的各个分量的范围均为： 0-255    */
		"  u = (texture2D(uv_texture, v_tex_coord).a * 16.0 * 15.0 + texture2D(uv_texture, v_tex_coord).r * 16.0) / 256.0;\n"
		"  v = (texture2D(uv_texture, v_tex_coord).g * 16.0 * 15.0 + texture2D(uv_texture, v_tex_coord).b * 16.0) / 256.0;\n"

		// "  gl_FragColor.b = y + 1.4075 * (v - 0.5);\n"
		// "  gl_FragColor.g = y - 0.3455 * (u - 0.5) - 0.7169 * (v - 0.5);\n"
		// "  gl_FragColor.r = y + 1.779 * (u - 0.5);\n"
		/* BT709 标准*/
		"  gl_FragColor.b = 1.164 * (y - sub) + 2.115 * (v - 0.5);\n"
		"  gl_FragColor.g = 1.164 * (y - sub) - 0.534 * (u - 0.5) - 0.213 * (v - 0.5);\n"
		"  gl_FragColor.r = 1.164 * (y - sub) + 1.793 * (u - 0.5);\n"

		"  gl_FragColor.a = 1.0;\n"
		"}                                                   \n";

	// Load the shaders and get a linked program object
	obj->shader = gl_load_program((const char*)v_shader_str, (const char*)f_shader_str);
	// Get the attribute locations
	obj->positions = glGetAttribLocation(obj->shader, "a_position");
	checkGlError(__FILE__, __LINE__);
	obj->tex_coord = glGetAttribLocation(obj->shader, "a_tex_coord");
	checkGlError(__FILE__, __LINE__);

	// Get the uniform locations
	obj->mvp = glGetUniformLocation(obj->shader, "u_mvp_matrix");
	checkGlError(__FILE__, __LINE__);

	obj->y_sampler = glGetUniformLocation(obj->shader, "y_texture");
	checkGlError(__FILE__, __LINE__);
	obj->uv_sampler = glGetUniformLocation(obj->shader, "uv_texture");
	checkGlError(__FILE__, __LINE__);
	return 0;
}

int nv12_bind_pbo(struct gl_texture *texture, char *data[4])
{
	gl_create_pbo_texture(&texture->Y_texture, texture->h_stride, texture->width, texture->height,
		data[0]);
	gl_create_pbo_texture(&texture->UV_texture, texture->h_stride, texture->width, texture->height / 2,
		data[1]);
	return 0;
} 	

int nv12_transport_pbo(struct gl_texture *texture, char *data[4])
{
	gl_update_pbo_texture(&texture->Y_texture, texture->h_stride, texture->width, texture->height,
		data[0]);
	gl_update_pbo_texture(&texture->UV_texture, texture->h_stride, texture->width, texture->height / 2,
		data[1]);
	return 0;
}

int nv12_bind_khr(struct gl_texture *texture, char *data[4])
{
	return 0;
}

int nv12_render(struct gl_object *obj, struct gl_texture *texture)
{
	glActiveTexture(GL_TEXTURE0);
	checkGlError(__FILE__, __LINE__);
	glBindTexture(GL_TEXTURE_2D, texture->Y_texture);
	checkGlError(__FILE__, __LINE__);
	glUniform1i (obj->y_sampler, 0);
	checkGlError(__FILE__, __LINE__);

	glActiveTexture(GL_TEXTURE1);
	checkGlError(__FILE__, __LINE__);
	glBindTexture(GL_TEXTURE_2D, texture->UV_texture);
	checkGlError(__FILE__, __LINE__);
	glUniform1i (obj->uv_sampler, 1);
	checkGlError(__FILE__, __LINE__);
	return 0;
}

int nv12_release(struct gl_object *obj)
{
	if(obj->shader)
	{
		glDeleteProgram(obj->shader);
		obj->shader = 0;
	}
	return 0;
}

struct shader_render_ops nv12_ops = 
{
	.init 			= nv12_init,
	.bind_pbo 		= nv12_bind_pbo,
	.transport_pbo 	= nv12_transport_pbo,
	.bind_khr 		= nv12_bind_khr,
	.render 		= nv12_render,
	.release		= nv12_release,
};