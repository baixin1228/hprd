common_source = files('queue.c', 'buffer_pool.c')

common_lib = static_library('common',
                        common_source,
                        include_directories : inc)

server_dep += common_lib