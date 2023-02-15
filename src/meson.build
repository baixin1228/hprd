inc = 'include'
server_src = []
server_dep = []

subdir('codec')
subdir('common')
subdir('output_dev')
subdir('input_dev')

server_dep += dependency('glib-2.0')

server_src += 'server.c'
server_src += 'output_dev.c'
server_src += 'input_dev.c'
server_src += 'encodec.c'
server_src += 'dl_help.c'
server_src += 'util.c'

hprd_server = executable('server', server_src,
	include_directories : inc,
	dependencies: server_dep,
	link_args : ['-ldl'],
    install : true)

test('hprd_server_test', hprd_server, is_parallel : false, timeout: 1000)