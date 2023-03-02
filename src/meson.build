inc = 'include'
server_srcs = []
client_srcs = []
common_deps = []
server_deps = []
client_deps = []
server_libs = []
client_libs = []
common_deps += dependency('glib-2.0')

subdir('codec')
subdir('common')
subdir('output_dev')
subdir('capture_dev')

server_deps += common_deps

server_srcs += 'server.c'
server_srcs += 'tcp_server.c'
server_srcs += 'capture_dev.c'
server_srcs += 'encodec.c'
server_srcs += 'dl_help.c'
server_srcs += 'util.c'

hprd_server = executable('server', server_srcs,
	include_directories : inc,
	dependencies: server_deps,
	link_with: server_libs,
	link_args : ['-ldl', '-lpthread'],
    install : true)

test('hprd_server_test', hprd_server, is_parallel : false, timeout: 1000)


client_deps += common_deps

client_srcs += 'client.c'
client_srcs += 'tcp_client.c'
client_srcs += 'output_dev.c'
client_srcs += 'decodec.c'
client_srcs += 'dl_help.c'
client_srcs += 'util.c'

hprd_client = executable('client', client_srcs,
	include_directories : inc,
	dependencies: client_deps,
	link_with: client_libs,
	link_args : ['-ldl', '-lpthread'],
    install : true)

test('hprd_client_test', hprd_client, is_parallel : false, timeout: 1000)