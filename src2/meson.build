subdir('libs')


subdir('output_dev/sdl_output')
subdir('input_dev/xcb_input')

src = []
src += 'server.c'
src += 'output_dev.c'
src += 'input_dev.c'
src += 'encodec.c'
src += 'dl_help.c'
src += 'util.c'

hprd_server = executable('server', src,
	include_directories : include_directories('include'),
	dependencies: [common_dep, dependency('glib-2.0')],
	link_args : ['-ldl'])

test('hprd_server_test', hprd_server, is_parallel : false, timeout: 1000)