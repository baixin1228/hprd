input_dev_source = files('xcb_input.c')

input_dev_lib = static_library('inputdev',
                        input_dev_source,
                        dependencies: [dependency('x11'),
                        dependency('xproto'), dependency('xext'),
                        dependency('glib-2.0')],
                        include_directories : '../include')

server_libs += input_dev_lib