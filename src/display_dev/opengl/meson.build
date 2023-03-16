gl_source = []
gl_source += 'gl_help.c'
gl_source += 'gl_render.c'
gl_source += 'nv12_render.c'
gl_source += 'yuv420_render.c'

gl_lib = static_library('gl-help',
                        gl_source,
						dependencies: [dependency('gl')],
                        include_directories : '../../include')