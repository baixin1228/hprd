display_dev_source = files('sdl_display.c')

display_dev_lib = static_library('displaydev',
                        display_dev_source,
						dependencies: [dependency('sdl2'),
						dependency('glib-2.0')],
                        include_directories : '../include')

client_libs += display_dev_lib