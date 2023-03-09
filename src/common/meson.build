common_source = []
common_source += 'queue.c'
common_source += 'buffer_pool.c'
common_source += 'buffer_util.c'
common_source += 'input_event.c'

common_lib = static_library('common',
                        common_source,
                        include_directories : '../include')

server_libs += common_lib
client_libs += common_lib