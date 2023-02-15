input_dev_source = files('xcb_input.c')

input_dev_lib = static_library('inputdev',
                        input_dev_source,
                        dependencies: [common_dep, dependency('x11'),
                        dependency('xproto'), dependency('xext'),
                        dependency('glib-2.0')],
                        include_directories : inc)

server_dep += input_dev_lib