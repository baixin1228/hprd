shared_library('sdloutput',
	'sdl_output.c',
	include_directories : ['../../include'],
	dependencies: [output_dev_dep, dependency('sdl2'), dependency('glib-2.0')],
    install : true)

output_dev_source = files('sdl_output.cc')

output_dev_lib = static_library('output_dev',
                        output_dev_source,
                        include_directories : inc)

server_dep += output_dev_lib