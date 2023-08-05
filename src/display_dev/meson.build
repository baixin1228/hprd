if get_option('sdl_client')
	display_dev_source = files('sdl_display.c')
	display_dev_lib = static_library('displaydev',
							display_dev_source,
							dependencies: [dependency('sdl2'),
							dependency('glib-2.0')],
							include_directories : '../include')

	client_libs += display_dev_lib
endif

subdir('opengl')
x11_renderer_source = files('x11_renderer.c')
x11_renderer_lib = static_library('x11renderer',
                        x11_renderer_source,
						dependencies: [dependency('x11'), dependency('egl'),
						dependency('libdrm'), dependency('glib-2.0')],
						link_whole : gl_lib,
                        include_directories : ['../include', 'opengl'])

client_libs += x11_renderer_lib