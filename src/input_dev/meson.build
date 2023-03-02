xlib_input_dev_source = files('xlib_input.c')

xlib_input_dev_lib = static_library('xlib_inputdev',
                        xlib_input_dev_source,
                        dependencies: [
                            common_deps,
                            dependency('xtst') #libxtst-dev,
                        ],
                        include_directories : '../include')

server_libs += xlib_input_dev_lib