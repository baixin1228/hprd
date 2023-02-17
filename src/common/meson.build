common_source = files('queue.c', 'buffer_pool.c')

common_lib = static_library('common',
                        common_source,
                        include_directories : '../include')

server_libs += common_lib