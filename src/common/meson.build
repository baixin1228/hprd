common_source = []
common_source += 'util.c'
common_source += 'queue.c'
common_source += 'buffer_pool.c'
common_source += 'buffer_util.c'
common_source += 'frame_buffer.c'
common_source += 'drm_allocator.c'

server_common_lib = static_library('common-server',
                        [common_source, 'protocol_server.c'],
                        dependencies: [common_deps, dependency('libdrm')],
                        include_directories : '../include')

client_common_lib = static_library('common-client',
                        [common_source, 'protocol_client.c'],
                        dependencies: [common_deps, dependency('libdrm')],
                        include_directories : '../include')

server_libs += server_common_lib
client_libs += client_common_lib