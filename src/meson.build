inc = 'include'
server_srcs = []
client_srcs = []
common_deps = []
server_deps = []
client_deps = []
server_libs = []
client_libs = []
common_deps += dependency('glib-2.0')

build_target = get_option('build_target')

subdir('net')
subdir('codec')
subdir('common')

if build_target == 'server' or build_target == 'all'
	subdir('capture_dev')
	subdir('input_dev')

	server_deps += common_deps

	server_srcs += 'input_dev.c'
	server_srcs += 'capture_dev.c'
	server_srcs += 'encodec.c'

	server_static_lib = static_library('server', server_srcs,
		include_directories : inc,
		dependencies: server_deps,
		link_whole: server_libs,
		link_args : ['-ldl', '-lpthread'],
		install : false)

	hprd_server = executable('server', 'server.c',
		include_directories : inc,
		dependencies: server_deps,
		link_whole: server_static_lib,
		link_args : ['-ldl', '-lpthread'],
		install : true)

	subdir('server_tests')
endif

if build_target == 'client' or build_target == 'all'
	subdir('display_dev')

	client_deps += common_deps

	client_srcs += 'display_dev.c'
	client_srcs += 'decodec.c'

	client_static_lib = static_library('client', client_srcs,
			include_directories : inc,
			dependencies: client_deps,
			link_whole: client_libs,
			link_args : ['-ldl', '-lpthread'],
		    install : false)

	if get_option('sdl_client')
		hprd_client = executable('client', 'client.c',
			include_directories : inc,
			dependencies: client_deps,
			link_whole: client_static_lib,
			link_args : ['-ldl', '-lpthread'],
			install : false)
		test('hprd_client_test', hprd_client, is_parallel : false, timeout: 1000)
	endif

	subdir('clients/pyqt')
endif
