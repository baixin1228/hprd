output_dev_source = files('sdl_output.c')

output_dev_lib = static_library('outputdev',
                        output_dev_source,
						dependencies: [dependency('sdl2'),
						dependency('glib-2.0')],
                        include_directories : '../include')

server_libs += output_dev_lib