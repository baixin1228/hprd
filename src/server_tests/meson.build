encodec_test = executable('encodec_test', 'encodec_test.c',
	include_directories : '../include',
	dependencies: server_deps,
	link_with: server_static_lib,
	link_args : ['-ldl', '-lpthread'],
    install : false)

test('encodec_test', encodec_test, is_parallel : false, timeout: 10)
