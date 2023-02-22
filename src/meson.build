inc = 'include'
server_srcs = []
common_deps = []
server_deps = []
server_libs = []
common_deps += dependency('glib-2.0')

subdir('codec')
subdir('common')
subdir('output_dev')
subdir('input_dev')

server_deps += common_deps

server_srcs += 'server.c'
server_srcs += 'output_dev.c'
server_srcs += 'input_dev.c'
server_srcs += 'encodec.c'
server_srcs += 'decodec.c'
server_srcs += 'dl_help.c'
server_srcs += 'util.c'

hprd_server = executable('server', server_srcs,
	include_directories : inc,
	dependencies: server_deps,
	link_with: server_libs,
	link_args : ['-ldl'],
    install : true)

test('hprd_server_test', hprd_server, is_parallel : false, timeout: 1000)