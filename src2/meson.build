src = ['server.c', 'output_dev.c', 'dl_help.c']


hprd_server = executable('server', src,
	include_directories : include_directories('include'),
	link_args : ['-ldl'])

test('hprd_server_test', hprd_server, is_parallel : false)
