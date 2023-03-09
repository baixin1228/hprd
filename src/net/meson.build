server_net_source = []
server_net_source += 'tcp_server.c'
#server_net_source += 'udp_server.c'
server_net_source += 'net_help.c'

server_net_lib = static_library('server-net',
                        server_net_source,
                        dependencies: [common_deps ],
                        include_directories : '../include')

server_libs += server_net_lib

client_net_source = []
client_net_source += 'tcp_client.c'
#client_net_source += 'udp_client.c'
client_net_source += 'net_help.c'

client_net_lib = static_library('client-net',
                        client_net_source,
                        dependencies: [common_deps ],
                        include_directories : '../include')

client_libs += client_net_lib