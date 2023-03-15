
void init_rgb_programger(struct gl_object *obj)
{
	struct user_data *obj = &ctx->obj;

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
		"uniform sampler2D s_texture;                        \n"
		"void main()                                         \n"
		"{                                                   \n"
		"  gl_FragColor.r = texture2D(s_texture, v_texCoord).r;\n"
		"  gl_FragColor.g = texture2D(s_texture, v_texCoord).r;\n"
		"  gl_FragColor.b = texture2D(s_texture, v_texCoord).r;\n"
		"}                                                   \n";

	// Load the shaders and get a linked program object
	obj->shader = gl_load_program((const char*)vShaderStr, (const char*)fShaderStr);
	checkGlError(__FILE__, __LINE__);

	// Get the attribute locations
	obj->positions = glGetAttribLocation(obj->shader, "a_position");
	checkGlError(__FILE__, __LINE__);
	obj->tex_coord = glGetAttribLocation(obj->shader, "a_texCoord");
	checkGlError(__FILE__, __LINE__);

	// Get the uniform locations
	obj->mvp = glGetUniformLocation(obj->shader, "u_mvp_matrix");
	checkGlError(__FILE__, __LINE__);
	obj->samplerLoc = glGetUniformLocation(obj->shader, "s_texture");
	checkGlError(__FILE__, __LINE__);
}

GLuint g_planeIndices[] =
{
	0, 3, 1, //front plane
	0, 2, 3,
};

int gl_render(struct gl_obj *gl, EGLContext context)
{
	struct gl_surface *surface = &gl->surfaces[gl->render_idx];
	struct user_data *userData = &gl->userData;

	if(surface->drm_buffer == NULL)
	{
		line_error("gl_render drm buffer is null\n");
		return -1;
	}
	EGLContext cur_context = eglGetCurrentContext();

	if(!cur_context)
	{
		line_error("eglContext is null.\n");
		return -1;
	}
	if(cur_context != context)
	{
		line_error("eglContext is not same.\n");
		return -1;
	}
	switch(surface->drm_buffer->format)
	{
		case DRM_FORMAT_YUV420:
			glActiveTexture(GL_TEXTURE0);
			checkGlError(__FILE__, __LINE__);
			glBindTexture(GL_TEXTURE_2D, surface->Y_texture);
			checkGlError(__FILE__, __LINE__);
			glUniform1i (userData->y_sampler, 0);
			checkGlError(__FILE__, __LINE__);

			glActiveTexture(GL_TEXTURE1);
			checkGlError(__FILE__, __LINE__);
			glBindTexture(GL_TEXTURE_2D, surface->U_texture);
			checkGlError(__FILE__, __LINE__);
			glUniform1i (userData->u_sampler, 1);
			checkGlError(__FILE__, __LINE__);

			glActiveTexture(GL_TEXTURE2);
			checkGlError(__FILE__, __LINE__);
			glBindTexture(GL_TEXTURE_2D, surface->V_texture);
			checkGlError(__FILE__, __LINE__);
			glUniform1i (userData->v_sampler, 2);
			checkGlError(__FILE__, __LINE__);
		break;
		case DRM_FORMAT_RGB888:
			glActiveTexture(GL_TEXTURE0);
			checkGlError(__FILE__, __LINE__);
			glBindTexture(GL_TEXTURE_2D, gl->surfaces[gl->render_idx].texture);
			checkGlError(__FILE__, __LINE__);
			glUniform1i (userData->samplerLoc, 0);
			checkGlError(__FILE__, __LINE__);
		break;
		case DRM_FORMAT_NV12:
			glActiveTexture(GL_TEXTURE0);
			checkGlError(__FILE__, __LINE__);
			glBindTexture(GL_TEXTURE_2D, surface->Y_texture);
			checkGlError(__FILE__, __LINE__);
			glUniform1i (userData->y_sampler, 0);
			checkGlError(__FILE__, __LINE__);

			glActiveTexture(GL_TEXTURE1);
			checkGlError(__FILE__, __LINE__);
			glBindTexture(GL_TEXTURE_2D, surface->UV_texture);
			checkGlError(__FILE__, __LINE__);
			glUniform1i (userData->uv_sampler, 1);
			checkGlError(__FILE__, __LINE__);
		break;
	}

	glDrawElements (GL_TRIANGLES, 6 /*userData->numIndices*/, GL_UNSIGNED_INT, g_planeIndices);
	checkGlError(__FILE__, __LINE__);
	return 0;
}