output_dev_source = files('sdl_output.c')

output_dev_lib = static_library('outputdev',
                        output_dev_source,
						dependencies: [output_dev_dep, dependency('sdl2'),
						dependency('glib-2.0')],
                        include_directories : inc)

server_dep += output_dev_lib