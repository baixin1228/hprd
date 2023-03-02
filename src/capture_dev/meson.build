capture_dev_source = files('xcb_capture.c')

capture_dev_lib = static_library('capturedev',
                        capture_dev_source,
                        dependencies: [
                            common_deps,
                            dependency('x11'),
                            dependency('xproto'),
                            dependency('xext')
                        ],
                        include_directories : '../include')

server_libs += capture_dev_lib