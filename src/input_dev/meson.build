input_dev_source = files('xcb_input.c')

input_dev_lib = static_library('inputdev',
                        input_dev_source,
                        dependencies: [
                            common_deps,
                            dependency('x11'),
                            dependency('xproto'),
                            dependency('xext')
                        ],
                        include_directories : '../include')

server_libs += input_dev_lib